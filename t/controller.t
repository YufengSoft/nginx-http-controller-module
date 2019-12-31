#!/usr/bin/perl

# (C) hongzhidao

# Tests for nginx controller module.

###############################################################################

use warnings;
use strict;

use Test::More;
use Socket qw/ CRLF /;

BEGIN { use FindBin; chdir($FindBin::Bin); }

use lib 'lib';
use Test::Nginx;

###############################################################################

select STDERR; $| = 1;
select STDOUT; $| = 1;

my $t = Test::Nginx->new()->has(qw/http controller/)->plan(15);

$t->write_file_expand('nginx.conf', <<'EOF');

%%TEST_GLOBALS%%

daemon off;

events {
}

http {
    %%TEST_GLOBALS_HTTP%%

    ctrl_zone  zone=controller:10M;

    server {
        listen       127.0.0.1:8080;
        server_name  localhost;

        location / {
            ctrl  on;
        }

        location /config {
            ctrl_config;
        }
    }
}

EOF

$t->write_file('index.html', 'test ok');

$t->write_file('conf.json', <<EOF);
{
    "routes": [
        {
            "action": {
                "blacklist": [
                    "127.0.0.2"
                ],

                "whitelist": "127.0.0.1"
            }
        }
    ]
}
EOF


$t->run();


###############################################################################

like(http_get('/'), qr/test ok/, '200 ok');

like(http_get('/config/routes/0/action/blacklist'), qr/127.0.0.2/, 'GET configuration');
like(http_get('/config/routes/0/action/whitelist'), qr/127.0.0.1/, 'GET configuration');

like(http_put('/config/routes/0/action/blacklist/0', '"127.0.0.1"'), qr/Reconfiguration done/, 'PUT configuration');
like(http_get('/'), qr/403/, 'forbidden');

like(http_put('/config/routes/0/action/blacklist/0', '"127.0.0.2"'), qr/Reconfiguration done/, 'PUT configuration');
like(http_get('/'), qr/test ok/, '200 ok');

like(http_delete('/config/routes/0/action/blacklist'), qr/Reconfiguration done/, 'DELETE configuration');
like(http_get('/'), qr/test ok/, '200 ok');

like(http_put('/config/routes/0/action/whitelist', '"127.0.0.2"'), qr/Reconfiguration done/, 'PUT configuration');
like(http_get('/'), qr/403/, 'forbidden');

like(http_put('/config/routes/0/match', '{"host": "somehost"}'), qr/Reconfiguration done/, 'PUT configuration');
like(http_get('/'), qr/test ok/, '200 ok');

like(http_put('/config/routes/0/match/host', '"localhost"'), qr/Reconfiguration done/, 'PUT configuration');
like(http_get('/'), qr/403/, 'forbidden');

###############################################################################

sub http_put {
    my ($url, $body) = @_;
    my $len = length($body);

    my $p = "PUT $url HTTP/1.0" . CRLF .
        "Host: localhost" . CRLF .
        "Content-Length: $len" . CRLF .
        CRLF .
        "$body";

    return http($p);
}

sub http_delete {
    my ($url) = @_;

    my $p = "DELETE $url HTTP/1.0" . CRLF .
        "Host: localhost" . CRLF .
        CRLF;

    return http($p);
}

###############################################################################
