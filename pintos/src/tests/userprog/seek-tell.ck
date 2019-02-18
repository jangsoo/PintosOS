# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(seek-tell) begin
(seek-tell) filesize: 239
(seek-tell) tell: 0
(seek-tell) tell: 100
(seek-tell) tell: 200
(seek-tell) tell: 339
(seek-tell) 0 bytes read
(seek-tell) tell: 339
(seek-tell) end
seek-tell: exit(0)
EOF
pass;
