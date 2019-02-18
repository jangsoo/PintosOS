# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(filesize) begin
(filesize) end
filesize: exit(0)
EOF
pass;