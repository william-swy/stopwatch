#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stopwatch/stopwatch.h"
#include "str_table.h"
#include <papi.h>

#define NUM_OF_EVENTS 2
#define INDENT_SPACING 4

// Flag to signal initialization
static bool initialized_stopwatch = false;

// =====================================================================================================================
// PAPI specific variables
// =====================================================================================================================
static int events[NUM_OF_EVENTS] = {PAPI_L1_TCM, PAPI_RES_STL};
static int event_set = PAPI_NULL;
static long long results[NUM_OF_EVENTS] = {0, 0};

// =====================================================================================================================
// Structure used to hold readings from the measurement clock.
// =====================================================================================================================

// Think there might be an extra 4 byte padding for alignment reasons
struct MeasurementReadings {
  long long total_real_cyc;
  long long total_real_usec;
  long long total_l1_misses;
  long long total_cyc_wait_resource;

  long long total_times_called;

  long long start_real_cyc;
  long long start_real_usec;
  long long start_l1_misses;
  long long start_cyc_wait_resource;

  char routine_name[NULL_TERM_MAX_ROUTINE_NAME_LEN];
  unsigned int stack_depth;
};

#define STOPWATCH_MAX_FUNCTION_CALLS 500
static struct MeasurementReadings readings[STOPWATCH_MAX_FUNCTION_CALLS];

// =====================================================================================================================
// Operations
// =====================================================================================================================

// Initializes PAPI library. Should only be called once.
// Will return STOPWATCH_OK if successful
// Will return STOPWATCH_ERR if fail
int init_event_timers() {
  // Check if stopwatch is not already initialized
  if (!initialized_stopwatch) {
    // Reset the values in readings with the default values
    for (unsigned int idx = 0; idx < STOPWATCH_MAX_FUNCTION_CALLS; idx++) {
      readings[idx].total_real_cyc = 0;
      readings[idx].total_real_usec = 0;
      readings[idx].total_l1_misses = 0;
      readings[idx].total_cyc_wait_resource = 0;

      readings[idx].total_times_called = 0;
      readings[idx].stack_depth = 0;
      memset(&readings[idx].routine_name, 0, sizeof(readings[idx].routine_name));
    }

    int ret_val = PAPI_library_init(PAPI_VER_CURRENT);
    if (ret_val != PAPI_VER_CURRENT) {
      return STOPWATCH_ERR;
    }

    ret_val = PAPI_create_eventset(&event_set);
    if (ret_val != PAPI_OK) {
      return STOPWATCH_ERR;
    }

    ret_val = PAPI_add_events(event_set, events, NUM_OF_EVENTS);
    if (ret_val != PAPI_OK) {
      return STOPWATCH_ERR;
    }

    // Start the monotonic clock
    ret_val = PAPI_start(event_set);
    if (ret_val != PAPI_OK) {
      return STOPWATCH_ERR;
    }

    initialized_stopwatch = true;

    return STOPWATCH_OK;
  }
  return STOPWATCH_ERR;
}

int destroy_event_timers() {
  int ret_val = PAPI_stop(event_set, results);
  if (ret_val != PAPI_OK) {
    return STOPWATCH_ERR;
  }

  ret_val = PAPI_cleanup_eventset(event_set);
  if (ret_val != PAPI_OK) {
    return STOPWATCH_ERR;
  }
  ret_val = PAPI_destroy_eventset(&event_set);
  if (ret_val != PAPI_OK) {
    return STOPWATCH_ERR;
  }

  PAPI_shutdown();

  initialized_stopwatch = false;

  return STOPWATCH_OK;
}

int record_start_measurements(int routine_call_num, const char *function_name, unsigned int stack_depth) {
  int PAPI_ret = PAPI_read(event_set, results);
  if (PAPI_ret != PAPI_OK) {
    return STOPWATCH_ERR;
  }
  readings[routine_call_num].start_real_cyc = PAPI_get_real_cyc();
  readings[routine_call_num].start_real_usec = PAPI_get_real_usec();
  readings[routine_call_num].start_l1_misses = results[0];
  readings[routine_call_num].start_cyc_wait_resource = results[1];

  // Only log these values the first time it is called as there is a possibility of nesting.
  if (readings[routine_call_num].total_times_called == 0) {
    readings[routine_call_num].stack_depth = stack_depth;

    // Copy in the routine name and ensure the string is null terminated
    strncpy(readings[routine_call_num].routine_name, function_name, NULL_TERM_MAX_ROUTINE_NAME_LEN);
    readings[routine_call_num].routine_name[NULL_TERM_MAX_ROUTINE_NAME_LEN - 1] = '\0';
  }

  return STOPWATCH_OK;
}

int record_end_measurements(int routine_call_num) {
  int PAPI_ret = PAPI_read(event_set, results);
  if (PAPI_ret != PAPI_OK) {
    return STOPWATCH_ERR;
  }

  readings[routine_call_num].total_times_called++;

  readings[routine_call_num].total_real_cyc += (PAPI_get_real_cyc() - readings[routine_call_num].start_real_cyc);
  readings[routine_call_num].total_real_usec += (PAPI_get_real_usec() - readings[routine_call_num].start_real_usec);
  readings[routine_call_num].total_l1_misses += (results[0] - readings[routine_call_num].start_l1_misses);
  readings[routine_call_num].total_cyc_wait_resource +=
      (results[1] - readings[routine_call_num].start_cyc_wait_resource);

  return STOPWATCH_OK;
}

void print_measurement_results(struct MeasurementResult *result) {
  printf("Procedure name: %s\n", result->routine_name);
  printf("Total times run: %lld\n", result->total_times_called);
  printf("Total real cycles elapsed: %lld\n", result->total_real_cyc);
  printf("Total real microseconds elapsed: %lld\n", result->total_real_usec);
  printf("Total L1 cache misses: %lld\n", result->total_l1_misses);
  printf("Total cycles waiting for resources: %lld\n", result->total_cyc_wait_resource);
}

int get_measurement_results(unsigned int routine_call_num, struct MeasurementResult *result) {
  if (routine_call_num >= STOPWATCH_MAX_FUNCTION_CALLS) {
    return STOPWATCH_ERR;
  }

  result->total_real_cyc = readings[routine_call_num].total_real_cyc;
  result->total_real_usec = readings[routine_call_num].total_real_usec;
  result->total_l1_misses = readings[routine_call_num].total_l1_misses;
  result->total_cyc_wait_resource = readings[routine_call_num].total_cyc_wait_resource;

  result->total_times_called = readings[routine_call_num].total_times_called;
  result->stack_depth = readings[routine_call_num].stack_depth;

  // String already null terminated
  strncpy(result->routine_name, readings[routine_call_num].routine_name, NULL_TERM_MAX_ROUTINE_NAME_LEN);

  return STOPWATCH_OK;
}

// =====================================================================================================================
// Print results into a formatted table
// =====================================================================================================================

static unsigned int find_num_entries() {
  unsigned int entries = 0;
  for (unsigned int idx = 0; idx < STOPWATCH_MAX_FUNCTION_CALLS; idx++) {
    if (readings[idx].total_times_called > 0) {
      entries++;
    }
  }
  return entries;
}

void print_result_table() {
  // Generate table
  const unsigned int columns = 7;
  const unsigned int rows = find_num_entries() + 1; // Extra row for header

  struct StringTable *table = create_table(columns, rows, true, INDENT_SPACING);

  add_entry_str(table, "ID", (struct StringTableCellPos){0, 0});
  add_entry_str(table, "NAME", (struct StringTableCellPos){0, 1});
  add_entry_str(table, "TIMES CALLED", (struct StringTableCellPos){0, 2});
  add_entry_str(table, "TOTAL REAL CYCLES", (struct StringTableCellPos){0, 3});
  add_entry_str(table, "TOTAL REAL MICROSECONDS", (struct StringTableCellPos){0, 4});
  add_entry_str(table, "TOTAL L1 CACHE MISSES", (struct StringTableCellPos){0, 5});
  add_entry_str(table, "TOTAL CYCLES WAITING FOR RESOURCES", (struct StringTableCellPos){0, 6});

  unsigned int row_cursor = 1;
  for (unsigned int idx = 0; idx < STOPWATCH_MAX_FUNCTION_CALLS; idx++) {
    if (readings[idx].total_times_called == 0) {
      continue;
    }
    add_entry_lld(table, idx, (struct StringTableCellPos){row_cursor, 0});

    add_entry_str(table, readings[idx].routine_name, (struct StringTableCellPos){row_cursor, 1});
    set_indent_lvl(table, readings[idx].stack_depth, (struct StringTableCellPos){row_cursor, 1});

    add_entry_lld(table, readings[idx].total_times_called, (struct StringTableCellPos){row_cursor, 2});
    add_entry_lld(table, readings[idx].total_real_cyc, (struct StringTableCellPos){row_cursor, 3});
    add_entry_lld(table, readings[idx].total_real_usec, (struct StringTableCellPos){row_cursor, 4});
    add_entry_lld(table, readings[idx].total_l1_misses, (struct StringTableCellPos){row_cursor, 5});
    add_entry_lld(table, readings[idx].total_cyc_wait_resource, (struct StringTableCellPos){row_cursor, 6});

    row_cursor++;
  }

  // Format print table
  char* table_str = make_table_str(table);
  printf("%s\n", table_str);
  free(table_str);
  destroy_table(table);
}