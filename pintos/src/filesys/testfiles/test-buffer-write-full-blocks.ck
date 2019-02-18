# -*- perl -*-
use strict;
use warnings;
use tests::tests;
use tests::random;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(test-buffer-cache-effectiveness) begin
(test-buffer-cache-effectiveness) clearing cache
(test-buffer-cache-effectiveness) read_cnt before
(test-buffer-cache-effectiveness) write_cnt before
(test-buffer-cache-effectiveness) create "please"
(test-buffer-cache-effectiveness) open "please"
(test-buffer-cache-effectiveness) writing "please"
(test-buffer-cache-effectiveness) close "please"
(test-buffer-cache-effectiveness) open "please" for verification
(test-buffer-cache-effectiveness) verified contents of "please"
(test-buffer-cache-effectiveness) close "please"
(test-buffer-cache-effectiveness) reading "please" second
(test-buffer-cache-effectiveness) read_cnt after
(test-buffer-cache-effectiveness) write_cnt after
(test-buffer-cache-effectiveness) read_cnt and write_cnt within reasonable parameters
(test-buffer-cache-effectiveness) end
EOF
pass;
