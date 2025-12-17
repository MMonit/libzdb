/*
 * Copyright (C) Tildeslash Ltd. All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 *
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.
 */

#ifndef SQLSTATE_INCLUDED
#define SQLSTATE_INCLUDED

/**
 * @brief **SQLState** provides PostgreSQL error codes for use with libzdb.
 *
 * This interface provides SQLSTATE error code constants for PostgreSQL. When
 * using libzdb with PostgreSQL, `Exception_frame.errorCode` will contain
 * one of these values, allowing applications to handle specific database
 * errors programmatically.
 *
 * SQLSTATE is a five-character error code defined by the SQL standard
 * (ISO/IEC 9075). Each code consists of a two-character class (error category)
 * followed by a three-character subclass (specific condition). The codes are
 * encoded as integers using base-36 arithmetic for efficient comparison.
 *
 * **Note:** This header is intended for use with PostgreSQL only. For MySQL,
 * SQLite, or Oracle, use the native error codes provided by those database
 * systems. See the Exception.h documentation for details on error handling
 * with different database backends.
 *
 * ## Example Usage
 *
 * ```c
 * TRY
 *     Connection_execute(con, "INSERT INTO users (ssn)...");
 * ELSE
 *     if (Exception_frame.errorCode == SQLSTATE_unique_violation) {
 *         printf("SSN already exists\n");
 *     } else if (Exception_frame.errorCode == SQLSTATE_foreign_key_violation) {
 *         printf("Referenced record not found\n");
 *     } else if (Exception_frame.errorCode == SQLSTATE_deadlock_detected) {
 *         printf("Deadlock - consider retry\n");
 *     } else if (Exception_frame.errorCode == SQLSTATE_lock_not_available) {
 *         printf("Lock timeout - consider retry\n");
 *     } else {
 *         printf("Database error (%d): %s\n", Exception_frame.errorCode, Exception_frame.message);
 *     }
 * END_TRY;
 * ```
 *
 * ## Utility Functions
 *
 * - SQLState_toInt(): Convert a 5-character SQLSTATE string to its integer encoding
 * - SQLState_toString(): Convert an encoded integer back to its SQLSTATE string
 *
 * These functions are useful for debugging or when working with SQLSTATE
 * codes obtained from other sources.
 *
 * @see Exception.h
 * @see https://www.postgresql.org/docs/current/errcodes-appendix.html
 * @file
 */


/**
 * @brief Encode a SQLSTATE string as an integer.
 *
 * Converts a 5-character SQLSTATE code to a unique integer using base-36
 * encoding. Valid characters are 0-9 and A-Z. The encoding is reversible
 * via SQLState_toString().
 *
 * @param sqlstate A 5-character SQLSTATE string (e.g., "23505")
 * @return Encoded integer value, or 0 if sqlstate is NULL or invalid
 * @see SQLState_toString
 */
int SQLState_toInt(const char *sqlstate);


/**
 * @brief Decode an integer SQLSTATE back to its string representation.
 *
 * Converts an encoded SQLSTATE integer back to the original 5-character
 * string. The result is written to the provided buffer.
 *
 * @param code The encoded SQLSTATE integer
 * @param buf Buffer to receive the SQLSTATE string (must be at least 6 bytes)
 * @return The buf pointer, or NULL if code is 0
 * @see SQLState_toInt
 */
char *SQLState_toString(int code, char* buf);


/* ============================================================
 * PostgreSQL Error Codes (SQLSTATE)
 *
 * Complete list from PostgreSQL documentation Appendix A.
 * Values are pre-computed using SQLState_toInt() for efficiency.
 *
 * Reference: https://www.postgresql.org/docs/current/errcodes-appendix.html
 * ============================================================ */


/* --- Class 00: Successful Completion --- */
#define SQLSTATE_successful_completion                                         0  /* 00000 */

/* --- Class 01: Warning --- */
#define SQLSTATE_warning                                                   46656  /* 01000 */
#define SQLSTATE_dynamic_result_sets_returned                              46668  /* 0100C */
#define SQLSTATE_implicit_zero_bit_padding                                 46664  /* 01008 */
#define SQLSTATE_null_value_eliminated_in_set_function                     46659  /* 01003 */
#define SQLSTATE_privilege_not_granted                                     46663  /* 01007 */
#define SQLSTATE_privilege_not_revoked                                     46662  /* 01006 */
#define SQLSTATE_string_data_right_truncation_warning                      46660  /* 01004 */
#define SQLSTATE_deprecated_feature                                        79057  /* 01P01 */

/* --- Class 02: No Data --- */
#define SQLSTATE_no_data                                                   93312  /* 02000 */
#define SQLSTATE_no_additional_dynamic_result_sets_returned                93313  /* 02001 */

/* --- Class 03: SQL Statement Not Yet Complete --- */
#define SQLSTATE_sql_statement_not_yet_complete                           139968  /* 03000 */

/* --- Class 08: Connection Exception --- */
#define SQLSTATE_connection_exception                                     373248  /* 08000 */
#define SQLSTATE_connection_does_not_exist                                373251  /* 08003 */
#define SQLSTATE_connection_failure                                       373254  /* 08006 */
#define SQLSTATE_sqlclient_unable_to_establish_sqlconnection              373249  /* 08001 */
#define SQLSTATE_sqlserver_rejected_establishment_of_sqlconnection        373252  /* 08004 */
#define SQLSTATE_transaction_resolution_unknown                           373255  /* 08007 */
#define SQLSTATE_protocol_violation                                       405649  /* 08P01 */

/* --- Class 09: Triggered Action Exception --- */
#define SQLSTATE_triggered_action_exception                               419904  /* 09000 */

/* --- Class 0A: Feature Not Supported --- */
#define SQLSTATE_feature_not_supported                                    466560  /* 0A000 */

/* --- Class 0B: Invalid Transaction Initiation --- */
#define SQLSTATE_invalid_transaction_initiation                           513216  /* 0B000 */

/* --- Class 0F: Locator Exception --- */
#define SQLSTATE_locator_exception                                        699840  /* 0F000 */
#define SQLSTATE_invalid_locator_specification                            699841  /* 0F001 */

/* --- Class 0L: Invalid Grantor --- */
#define SQLSTATE_invalid_grantor                                          979776  /* 0L000 */
#define SQLSTATE_invalid_grant_operation                                 1012177  /* 0LP01 */

/* --- Class 0P: Invalid Role Specification --- */
#define SQLSTATE_invalid_role_specification                              1166400  /* 0P000 */

/* --- Class 0Z: Diagnostics Exception --- */
#define SQLSTATE_diagnostics_exception                                   1632960  /* 0Z000 */
#define SQLSTATE_stacked_diagnostics_accessed_without_active_handler     1632962  /* 0Z002 */

/* --- Class 20: Case Not Found --- */
#define SQLSTATE_case_not_found                                          3359232  /* 20000 */

/* --- Class 21: Cardinality Violation --- */
#define SQLSTATE_cardinality_violation                                   3405888  /* 21000 */

/* --- Class 22: Data Exception --- */
#define SQLSTATE_data_exception                                          3452544  /* 22000 */
#define SQLSTATE_array_subscript_error                                   3452630  /* 2202E */
#define SQLSTATE_character_not_in_repertoire                             3452617  /* 22021 */
#define SQLSTATE_datetime_field_overflow                                 3452552  /* 22008 */
#define SQLSTATE_division_by_zero                                        3452582  /* 22012 */
#define SQLSTATE_error_in_assignment                                     3452549  /* 22005 */
#define SQLSTATE_escape_character_conflict                               3452555  /* 2200B */
#define SQLSTATE_indicator_overflow                                      3452618  /* 22022 */
#define SQLSTATE_interval_field_overflow                                 3452585  /* 22015 */
#define SQLSTATE_invalid_argument_for_logarithm                          3452594  /* 2201E */
#define SQLSTATE_invalid_argument_for_ntile_function                     3452584  /* 22014 */
#define SQLSTATE_invalid_argument_for_nth_value_function                 3452586  /* 22016 */
#define SQLSTATE_invalid_argument_for_power_function                     3452595  /* 2201F */
#define SQLSTATE_invalid_argument_for_width_bucket_function              3452596  /* 2201G */
#define SQLSTATE_invalid_character_value_for_cast                        3452588  /* 22018 */
#define SQLSTATE_invalid_datetime_format                                 3452551  /* 22007 */
#define SQLSTATE_invalid_escape_character                                3452589  /* 22019 */
#define SQLSTATE_invalid_escape_octet                                    3452557  /* 2200D */
#define SQLSTATE_invalid_escape_sequence                                 3452621  /* 22025 */
#define SQLSTATE_nonstandard_use_of_escape_character                     3484950  /* 22P06 */
#define SQLSTATE_invalid_indicator_parameter_value                       3452580  /* 22010 */
#define SQLSTATE_invalid_parameter_value                                 3452619  /* 22023 */
#define SQLSTATE_invalid_preceding_or_following_size                     3452583  /* 22013 */
#define SQLSTATE_invalid_regular_expression                              3452591  /* 2201B */
#define SQLSTATE_invalid_row_count_in_limit_clause                       3452612  /* 2201W */
#define SQLSTATE_invalid_row_count_in_result_offset_clause               3452613  /* 2201X */
#define SQLSTATE_invalid_tablesample_argument                            3452633  /* 2202H */
#define SQLSTATE_invalid_tablesample_repeat                              3452632  /* 2202G */
#define SQLSTATE_invalid_time_zone_displacement_value                    3452553  /* 22009 */
#define SQLSTATE_invalid_use_of_escape_character                         3452556  /* 2200C */
#define SQLSTATE_most_specific_type_mismatch                             3452560  /* 2200G */
#define SQLSTATE_null_value_not_allowed                                  3452548  /* 22004 */
#define SQLSTATE_null_value_no_indicator_parameter                       3452546  /* 22002 */
#define SQLSTATE_numeric_value_out_of_range                              3452547  /* 22003 */
#define SQLSTATE_sequence_generator_limit_exceeded                       3452561  /* 2200H */
#define SQLSTATE_string_data_length_mismatch                             3452622  /* 22026 */
#define SQLSTATE_string_data_right_truncation                            3452545  /* 22001 */
#define SQLSTATE_substring_error                                         3452581  /* 22011 */
#define SQLSTATE_trim_error                                              3452623  /* 22027 */
#define SQLSTATE_unterminated_c_string                                   3452620  /* 22024 */
#define SQLSTATE_zero_length_character_string                            3452559  /* 2200F */
#define SQLSTATE_floating_point_exception                                3484945  /* 22P01 */
#define SQLSTATE_invalid_text_representation                             3484946  /* 22P02 */
#define SQLSTATE_invalid_binary_representation                           3484947  /* 22P03 */
#define SQLSTATE_bad_copy_file_format                                    3484948  /* 22P04 */
#define SQLSTATE_untranslatable_character                                3484949  /* 22P05 */
#define SQLSTATE_not_an_xml_document                                     3452565  /* 2200L */
#define SQLSTATE_invalid_xml_document                                    3452566  /* 2200M */
#define SQLSTATE_invalid_xml_content                                     3452567  /* 2200N */
#define SQLSTATE_invalid_xml_comment                                     3452572  /* 2200S */
#define SQLSTATE_invalid_xml_processing_instruction                      3452573  /* 2200T */
#define SQLSTATE_duplicate_json_object_key_value                         3452652  /* 22030 */
#define SQLSTATE_invalid_argument_for_sql_json_datetime_function         3452653  /* 22031 */
#define SQLSTATE_invalid_json_text                                       3452654  /* 22032 */
#define SQLSTATE_invalid_sql_json_subscript                              3452655  /* 22033 */
#define SQLSTATE_more_than_one_sql_json_item                             3452656  /* 22034 */
#define SQLSTATE_no_sql_json_item                                        3452657  /* 22035 */
#define SQLSTATE_non_numeric_sql_json_item                               3452658  /* 22036 */
#define SQLSTATE_non_unique_keys_in_a_json_object                        3452659  /* 22037 */
#define SQLSTATE_singleton_sql_json_item_required                        3452660  /* 22038 */
#define SQLSTATE_sql_json_array_not_found                                3452661  /* 22039 */
#define SQLSTATE_sql_json_member_not_found                               3452662  /* 2203A */
#define SQLSTATE_sql_json_number_not_found                               3452663  /* 2203B */
#define SQLSTATE_sql_json_object_not_found                               3452664  /* 2203C */
#define SQLSTATE_too_many_json_array_elements                            3452665  /* 2203D */
#define SQLSTATE_too_many_json_object_members                            3452666  /* 2203E */
#define SQLSTATE_sql_json_scalar_required                                3452667  /* 2203F */
#define SQLSTATE_sql_json_item_cannot_be_cast_to_target_type             3452668  /* 2203G */

/* --- Class 23: Integrity Constraint Violation --- */
#define SQLSTATE_integrity_constraint_violation                          3499200  /* 23000 */
#define SQLSTATE_restrict_violation                                      3499201  /* 23001 */
#define SQLSTATE_not_null_violation                                      3505682  /* 23502 */
#define SQLSTATE_foreign_key_violation                                   3505683  /* 23503 */
#define SQLSTATE_unique_violation                                        3505685  /* 23505 */
#define SQLSTATE_check_violation                                         3505720  /* 23514 */
#define SQLSTATE_exclusion_violation                                     3531601  /* 23P01 */

/* --- Class 24: Invalid Cursor State --- */
#define SQLSTATE_invalid_cursor_state                                    3545856  /* 24000 */

/* --- Class 25: Invalid Transaction State --- */
#define SQLSTATE_invalid_transaction_state                               3592512  /* 25000 */
#define SQLSTATE_active_sql_transaction                                  3592513  /* 25001 */
#define SQLSTATE_branch_transaction_already_active                       3592514  /* 25002 */
#define SQLSTATE_held_cursor_requires_same_isolation_level               3592520  /* 25008 */
#define SQLSTATE_inappropriate_access_mode_for_branch_transaction        3592515  /* 25003 */
#define SQLSTATE_inappropriate_isolation_level_for_branch_transaction    3592516  /* 25004 */
#define SQLSTATE_no_active_sql_transaction_for_branch_transaction        3592517  /* 25005 */
#define SQLSTATE_read_only_sql_transaction                               3592518  /* 25006 */
#define SQLSTATE_schema_and_data_statement_mixing_not_supported          3592519  /* 25007 */
#define SQLSTATE_no_active_sql_transaction                               3624913  /* 25P01 */
#define SQLSTATE_in_failed_sql_transaction                               3624914  /* 25P02 */
#define SQLSTATE_idle_in_transaction_session_timeout                     3624915  /* 25P03 */
#define SQLSTATE_transaction_timeout                                     3624916  /* 25P04 */

/* --- Class 26: Invalid SQL Statement Name --- */
#define SQLSTATE_invalid_sql_statement_name                              3639168  /* 26000 */

/* --- Class 27: Triggered Data Change Violation --- */
#define SQLSTATE_triggered_data_change_violation                         3685824  /* 27000 */

/* --- Class 28: Invalid Authorization Specification --- */
#define SQLSTATE_invalid_authorization_specification                     3732480  /* 28000 */
#define SQLSTATE_invalid_password                                        3764881  /* 28P01 */

/* --- Class 2B: Dependent Privilege Descriptors Still Exist --- */
#define SQLSTATE_dependent_privilege_descriptors_still_exist             3872448  /* 2B000 */
#define SQLSTATE_dependent_objects_still_exist                           3904849  /* 2BP01 */

/* --- Class 2D: Invalid Transaction Termination --- */
#define SQLSTATE_invalid_transaction_termination                         3965760  /* 2D000 */

/* --- Class 2F: SQL Routine Exception --- */
#define SQLSTATE_sql_routine_exception                                   4059072  /* 2F000 */
#define SQLSTATE_function_executed_no_return_statement                   4059077  /* 2F005 */
#define SQLSTATE_modifying_sql_data_not_permitted_routine                4059074  /* 2F002 */
#define SQLSTATE_prohibited_sql_statement_attempted_routine              4059075  /* 2F003 */
#define SQLSTATE_reading_sql_data_not_permitted_routine                  4059076  /* 2F004 */

/* --- Class 34: Invalid Cursor Name --- */
#define SQLSTATE_invalid_cursor_name                                     5225472  /* 34000 */

/* --- Class 38: External Routine Exception --- */
#define SQLSTATE_external_routine_exception                              5412096  /* 38000 */
#define SQLSTATE_containing_sql_not_permitted                            5412097  /* 38001 */
#define SQLSTATE_modifying_sql_data_not_permitted                        5412098  /* 38002 */
#define SQLSTATE_prohibited_sql_statement_attempted                      5412099  /* 38003 */
#define SQLSTATE_reading_sql_data_not_permitted                          5412100  /* 38004 */

/* --- Class 39: External Routine Invocation Exception --- */
#define SQLSTATE_external_routine_invocation_exception                   5458752  /* 39000 */
#define SQLSTATE_invalid_sqlstate_returned                               5458753  /* 39001 */
#define SQLSTATE_null_value_not_allowed_external_routine                 5458756  /* 39004 */
#define SQLSTATE_trigger_protocol_violated                               5491153  /* 39P01 */
#define SQLSTATE_srf_protocol_violated                                   5491154  /* 39P02 */
#define SQLSTATE_event_trigger_protocol_violated                         5491155  /* 39P03 */

/* --- Class 3B: Savepoint Exception --- */
#define SQLSTATE_savepoint_exception                                     5552064  /* 3B000 */
#define SQLSTATE_invalid_savepoint_specification                         5552065  /* 3B001 */

/* --- Class 3D: Invalid Catalog Name --- */
#define SQLSTATE_invalid_catalog_name                                    5645376  /* 3D000 */

/* --- Class 3F: Invalid Schema Name --- */
#define SQLSTATE_invalid_schema_name                                     5738688  /* 3F000 */

/* --- Class 40: Transaction Rollback --- */
#define SQLSTATE_transaction_rollback                                    6718464  /* 40000 */
#define SQLSTATE_transaction_integrity_constraint_violation              6718466  /* 40002 */
#define SQLSTATE_serialization_failure                                   6718465  /* 40001 */
#define SQLSTATE_statement_completion_unknown                            6718467  /* 40003 */
#define SQLSTATE_deadlock_detected                                       6750865  /* 40P01 */

/* --- Class 42: Syntax Error or Access Rule Violation --- */
#define SQLSTATE_syntax_error_or_access_rule_violation                   6811776  /* 42000 */
#define SQLSTATE_syntax_error                                            6819553  /* 42601 */
#define SQLSTATE_insufficient_privilege                                  6818257  /* 42501 */
#define SQLSTATE_cannot_coerce                                           6822294  /* 42846 */
#define SQLSTATE_grouping_error                                          6822147  /* 42803 */
#define SQLSTATE_windowing_error                                         6844248  /* 42P20 */
#define SQLSTATE_invalid_recursion                                       6844221  /* 42P19 */
#define SQLSTATE_invalid_foreign_key                                     6822252  /* 42830 */
#define SQLSTATE_invalid_name                                            6819554  /* 42602 */
#define SQLSTATE_name_too_long                                           6819626  /* 42622 */
#define SQLSTATE_reserved_name                                           6823557  /* 42939 */
#define SQLSTATE_datatype_mismatch                                       6822148  /* 42804 */
#define SQLSTATE_indeterminate_datatype                                  6844220  /* 42P18 */
#define SQLSTATE_collation_mismatch                                      6844249  /* 42P21 */
#define SQLSTATE_indeterminate_collation                                 6844250  /* 42P22 */
#define SQLSTATE_wrong_object_type                                       6822153  /* 42809 */
#define SQLSTATE_generated_always                                        6822585  /* 428C9 */
#define SQLSTATE_undefined_column                                        6820851  /* 42703 */
#define SQLSTATE_undefined_function                                      6822435  /* 42883 */
#define SQLSTATE_undefined_table                                         6844177  /* 42P01 */
#define SQLSTATE_undefined_parameter                                     6844178  /* 42P02 */
#define SQLSTATE_undefined_object                                        6820852  /* 42704 */
#define SQLSTATE_duplicate_column                                        6820849  /* 42701 */
#define SQLSTATE_duplicate_cursor                                        6844179  /* 42P03 */
#define SQLSTATE_duplicate_database                                      6844180  /* 42P04 */
#define SQLSTATE_duplicate_function                                      6820923  /* 42723 */
#define SQLSTATE_duplicate_prepared_statement                            6844181  /* 42P05 */
#define SQLSTATE_duplicate_schema                                        6844182  /* 42P06 */
#define SQLSTATE_duplicate_table                                         6844183  /* 42P07 */
#define SQLSTATE_duplicate_alias                                         6820886  /* 42712 */
#define SQLSTATE_duplicate_object                                        6820884  /* 42710 */
#define SQLSTATE_ambiguous_column                                        6820850  /* 42702 */
#define SQLSTATE_ambiguous_function                                      6820925  /* 42725 */
#define SQLSTATE_ambiguous_parameter                                     6844184  /* 42P08 */
#define SQLSTATE_ambiguous_alias                                         6844185  /* 42P09 */
#define SQLSTATE_invalid_column_reference                                6844212  /* 42P10 */
#define SQLSTATE_invalid_column_definition                               6819589  /* 42611 */
#define SQLSTATE_invalid_cursor_definition                               6844213  /* 42P11 */
#define SQLSTATE_invalid_database_definition                             6844214  /* 42P12 */
#define SQLSTATE_invalid_function_definition                             6844215  /* 42P13 */
#define SQLSTATE_invalid_prepared_statement_definition                   6844216  /* 42P14 */
#define SQLSTATE_invalid_schema_definition                               6844217  /* 42P15 */
#define SQLSTATE_invalid_table_definition                                6844218  /* 42P16 */
#define SQLSTATE_invalid_object_definition                               6844219  /* 42P17 */

/* --- Class 44: WITH CHECK OPTION Violation --- */
#define SQLSTATE_with_check_option_violation                             6905088  /* 44000 */

/* --- Class 53: Insufficient Resources --- */
#define SQLSTATE_insufficient_resources                                  8538048  /* 53000 */
#define SQLSTATE_disk_full                                               8539344  /* 53100 */
#define SQLSTATE_out_of_memory                                           8540640  /* 53200 */
#define SQLSTATE_too_many_connections                                    8541936  /* 53300 */
#define SQLSTATE_configuration_limit_exceeded                            8543232  /* 53400 */

/* --- Class 54: Program Limit Exceeded --- */
#define SQLSTATE_program_limit_exceeded                                  8584704  /* 54000 */
#define SQLSTATE_statement_too_complex                                   8584705  /* 54001 */
#define SQLSTATE_too_many_columns                                        8584741  /* 54011 */
#define SQLSTATE_too_many_arguments                                      8584779  /* 54023 */

/* --- Class 55: Object Not In Prerequisite State --- */
#define SQLSTATE_object_not_in_prerequisite_state                        8631360  /* 55000 */
#define SQLSTATE_object_in_use                                           8631366  /* 55006 */
#define SQLSTATE_cant_change_runtime_param                               8663762  /* 55P02 */
#define SQLSTATE_lock_not_available                                      8663763  /* 55P03 */
#define SQLSTATE_unsafe_new_enum_value_usage                             8663764  /* 55P04 */

/* --- Class 57: Operator Intervention --- */
#define SQLSTATE_operator_intervention                                   8724672  /* 57000 */
#define SQLSTATE_query_canceled                                          8724712  /* 57014 */
#define SQLSTATE_admin_shutdown                                          8757073  /* 57P01 */
#define SQLSTATE_crash_shutdown                                          8757074  /* 57P02 */
#define SQLSTATE_cannot_connect_now                                      8757075  /* 57P03 */
#define SQLSTATE_database_dropped                                        8757076  /* 57P04 */
#define SQLSTATE_idle_session_timeout                                    8757077  /* 57P05 */

/* --- Class 58: System Error --- */
#define SQLSTATE_system_error                                            8771328  /* 58000 */
#define SQLSTATE_io_error                                                8771436  /* 58030 */
#define SQLSTATE_undefined_file                                          8803729  /* 58P01 */
#define SQLSTATE_duplicate_file                                          8803730  /* 58P02 */
#define SQLSTATE_file_name_too_long                                      8803731  /* 58P03 */

/* --- Class F0: Configuration File Error --- */
#define SQLSTATE_config_file_error                                      25194240  /* F0000 */
#define SQLSTATE_lock_file_exists                                       25194241  /* F0001 */

/* --- Class HV: Foreign Data Wrapper Error (SQL/MED) --- */
#define SQLSTATE_fdw_error                                              29999808  /* HV000 */
#define SQLSTATE_fdw_column_name_not_found                              29999813  /* HV005 */
#define SQLSTATE_fdw_dynamic_parameter_value_needed                     29999810  /* HV002 */
#define SQLSTATE_fdw_function_sequence_error                            29999844  /* HV010 */
#define SQLSTATE_fdw_inconsistent_descriptor_information                29999881  /* HV021 */
#define SQLSTATE_fdw_invalid_attribute_value                            29999884  /* HV024 */
#define SQLSTATE_fdw_invalid_column_name                                29999815  /* HV007 */
#define SQLSTATE_fdw_invalid_column_number                              29999816  /* HV008 */
#define SQLSTATE_fdw_invalid_data_type                                  29999812  /* HV004 */
#define SQLSTATE_fdw_invalid_data_type_descriptors                      29999814  /* HV006 */
#define SQLSTATE_fdw_invalid_descriptor_field_identifier                30000133  /* HV091 */
#define SQLSTATE_fdw_invalid_handle                                     29999819  /* HV00B */
#define SQLSTATE_fdw_invalid_option_index                               29999820  /* HV00C */
#define SQLSTATE_fdw_invalid_option_name                                29999821  /* HV00D */
#define SQLSTATE_fdw_invalid_string_length_or_buffer_length             30000132  /* HV090 */
#define SQLSTATE_fdw_invalid_string_format                              29999818  /* HV00A */
#define SQLSTATE_fdw_invalid_use_of_null_pointer                        29999817  /* HV009 */
#define SQLSTATE_fdw_too_many_handles                                   29999848  /* HV014 */
#define SQLSTATE_fdw_out_of_memory                                      29999809  /* HV001 */
#define SQLSTATE_fdw_no_schemas                                         29999833  /* HV00P */
#define SQLSTATE_fdw_option_name_not_found                              29999827  /* HV00J */
#define SQLSTATE_fdw_reply_handle                                       29999828  /* HV00K */
#define SQLSTATE_fdw_schema_not_found                                   29999834  /* HV00Q */
#define SQLSTATE_fdw_table_not_found                                    29999835  /* HV00R */
#define SQLSTATE_fdw_unable_to_create_execution                         29999829  /* HV00L */
#define SQLSTATE_fdw_unable_to_create_reply                             29999830  /* HV00M */
#define SQLSTATE_fdw_unable_to_establish_connection                     29999831  /* HV00N */

/* --- Class P0: PL/pgSQL Error --- */
#define SQLSTATE_plpgsql_error                                          41990400  /* P0000 */
#define SQLSTATE_raise_exception                                        41990401  /* P0001 */
#define SQLSTATE_no_data_found                                          41990402  /* P0002 */
#define SQLSTATE_too_many_rows                                          41990403  /* P0003 */
#define SQLSTATE_assert_failure                                         41990404  /* P0004 */

/* --- Class XX: Internal Error --- */
#define SQLSTATE_internal_error                                         56966976  /* XX000 */
#define SQLSTATE_data_corrupted                                         56966977  /* XX001 */
#define SQLSTATE_index_corrupted                                        56966978  /* XX002 */


#endif // !SQLSTATE_INCLUDED
