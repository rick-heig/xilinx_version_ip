# Original Idea credit goes to nolaega and a_bert
# Original link : https://forums.xilinx.com/t5/Vivado-TCL-Community/Vivado-TCL-set-generics-based-on-date-git-hash/td-p/426838
# Wayback Machine archive : https://web.archive.org/web/20200417104004/https://forums.xilinx.com/t5/Vivado-TCL-Community/Vivado-TCL-set-generics-based-on-date-git-hash/td-p/426838
# The original script has been updated by Rick Wertenbroek to the version below.

# Current date, time, and seconds since epoch
# 0 = 4-digit year
# 1 = 2-digit year
# 2 = 2-digit month
# 3 = 2-digit day
# 4 = 2-digit hour
# 5 = 2-digit minute
# 6 = 2-digit second
# 7 = Epoch (seconds since 1970-01-01_00:00:00)
# Array index                                            0  1  2  3  4  5  6  7
set datetime_arr [clock format [clock seconds] -format {%Y %y %m %d %H %M %S 00}]
# Example :
# 2020 20 05 27 13 45 45 00
 
# Get the datecode in the yyyy-mm-dd format
set datecode [lindex $datetime_arr 0][lindex $datetime_arr 2][lindex $datetime_arr 3]
# Get the timecode in the hh-mm-ss-00 format
set timecode [lindex $datetime_arr 4][lindex $datetime_arr 5][lindex $datetime_arr 6][lindex $datetime_arr 7]
# Show this in the log
puts DATECODE=$datecode
puts TIMECODE=$timecode
 
# Get the git hashtag for this project
set curr_dir [pwd]
set proj_dir [get_property DIRECTORY [current_project]]
cd $proj_dir

if { [catch {exec git rev-parse --short=8 HEAD}] } {
    puts "No git version control in the $proj_dir directory"
    set git_hash 00000000
} else {
    set git_hash [exec git rev-parse --short=8 HEAD]
}
# Show this in the log
puts HASHCODE=$git_hash
 
# Update the generics
set initial_generics [get_property generic [current_fileset]]
set_property generic "$initial_generics G_DATE_CODE=32'h$datecode G_TIME_CODE=32'h$timecode G_HASH_CODE=32'h$git_hash" [current_fileset]
