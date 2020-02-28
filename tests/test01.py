
import os
from lib.control import TestControl 


class TestCtrl(TestControl):

    def setUp(self):

        super().setUp('''
        error_log  logs/error.log debug;
        events {}
        http {
            ctrl_zone  zone=controller:10M;
            ctrl_set  $test "test";
            ctrl  on;

            server {
                listen  127.0.0.1:7080;

                location / {
                    proxy_set_header  "test" $test;
                    proxy_pass  http://127.0.0.1:7081;
                }
            }

            server {
                listen  127.0.0.1:7081;

                return  200 $http_test;
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
            self.get()['body'], 'test', 'init'
        )

        self.assertIn(
            'success',
            self.conf(
                {
                    "routes": [
                        {
                            "match": {"method": "GET"},
                            "action": {
                                "variables": {
                                    "test": "test00"
                                },
                            },
                        }
                    ],
                }
            ),
            'routing configure',
        )

        self.assertEqual(
            self.get()['body'], 'test00', 'update'
        )

    def route(self, route):
        return self.conf([route], 'routes')

    def route_match(self, match):
        self.assertIn(
            'success',
            self.route(
                {"match": match, "action": {"variables": {"test": "test01"}}}
            ),
            'route match configure',
        )

    def test_route_match_method_positive(self):
        self.route_match({"method": ["GET", "POST"]})

        self.assertEqual(self.get()['status'], 200, 'GET')
        self.assertEqual(self.get()['body'], 'test01', 'GET')
        self.assertEqual(self.delete()['status'], 200, 'DELETE')
        self.assertEqual(self.delete()['body'], 'test', 'DELETE')

    def test_route_match_method_negative(self):
        self.route_match({"method": "!GET"})

        self.assertEqual(self.get()['status'], 200, 'GET')
        self.assertEqual(self.get()['body'], 'test', 'GET')
        self.assertEqual(self.delete()['status'], 200, 'DELETE')
        self.assertEqual(self.delete()['body'], 'test01', 'DELETE')

    def test_route_rules_two(self):
        self.assertIn(
            'success',
            self.conf(
                [
                    {
                        "match": {"method": "GET"},
                        "action": {"variables": {"test": "test02"}},
                    },
                    {
                        "match": {"method": "POST"},
                        "action": {"variables": {"test": "test03"}},
                    },
                ],
                'routes',
            ),
            'rules two configure',
        )

        self.assertEqual(self.get()['body'], 'test02', 'rules two match first')
        self.assertEqual(
            self.post(
                headers={
                    'Host': 'localhost',
                    'Content-Type': 'text/html',
                    'Connection': 'close',
                },
                body='X',
            )['body'],
            'test03',
            'rules two match second',
        )


if __name__ == '__main__':
    TestCtrl.main()
