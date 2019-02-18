# -*- perl -*-
use strict;
use warnings;
use tests::tests;
use tests::random;
check_archive ({"please" => [random_bytes (2048)]});
pass;
