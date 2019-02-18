# -*- perl -*-
use strict;
use warnings;
use tests::tests;
use tests::random;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(test-buffer-cache-effectiveness) begin
(test-buffer-cache-effectiveness) create "insert_some_local_file_name"
(test-buffer-cache-effectiveness) open "insert_some_local_file_name"
(test-buffer-cache-effectiveness) writing "insert_some_local_file_name"
(test-buffer-cache-effectiveness) close "insert_some_local_file_name"
(test-buffer-cache-effectiveness) open "insert_some_local_file_name" for verification
(test-buffer-cache-effectiveness) contents of "insert_some_local_file_name" verified
(test-buffer-cache-effectiveness) close "insert_some_local_file_name"
(test-buffer-cache-effectiveness) clear cache
(test-buffer-cache-effectiveness) read count zero
(test-buffer-cache-effectiveness) open "insert_some_local_file_name"
(test-buffer-cache-effectiveness) read "insert_some_local_file_name" first
(test-buffer-cache-effectiveness) read count first
(test-buffer-cache-effectiveness) close "insert_some_local_file_name"
(test-buffer-cache-effectiveness) open "insert_some_local_file_name"
(test-buffer-cache-effectiveness) read "insert_some_local_file_name" second
(test-buffer-cache-effectiveness) read count second
(test-buffer-cache-effectiveness) close "insert_some_local_file_name"
(test-buffer-cache-effectiveness) read count to first was more than read count to second
(test-buffer-cache-effectiveness) end
EOF
pass;
