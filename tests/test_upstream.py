
import os
from lib.control import TestControl 


class TestCtrl(TestControl):

    def setUp(self):

        super().setUp('''
        error_log  logs/error.log debug;
        events {}
        http {
            ctrl_zone  zone=controller:10M;
            ctrl  on;

            upstream backend {
                zone  one 64K;

                server  127.0.0.1:7081;
                server  0.0.0.0:80;
                server  1.1.1.1:80;
                server  2.2.2.2:80 backup;
            }

            server {
                listen  127.0.0.1:7080;

                location / {
                    proxy_pass  http://backend;
                }
            }

            server {
                listen  127.0.0.1:7081;

                return  200 "backend1";
            }

            server {
                listen  127.0.0.1:7082;

                return  200 "backend2";
            }

            server {
                listen  127.0.0.1:7083;

                return  200 "backend3";
            }

            server {
                listen  127.0.0.1:7084;

                return  200 "backend4";
            }

            server {
                listen  127.0.0.1:8000;

                location /config {
                    ctrl  off;
                    ctrl_config;
                }
            }
        }
        ''')

        self.assertEqual(
            self.get()['body'], 'backend1', 'init'
        )

    def test_upstream(self):
        self.assertIn(
            'success',
            self.conf(
                {
                    "upstreams": {
                        "one": [
                            {
                                "address": "127.0.0.1:7082",
                            },
                            {
                                "address": "127.0.0.1:7083",
                            },
                            {
                                "address": "127.0.0.1:7084",
                            },
                        ]
                    },
                }
            ),
            'routing configure',
        )

        self.assertEqual(
            self.get()['body'], 'backend2', 'update'
        )


if __name__ == '__main__':
    TestCtrl.main()
