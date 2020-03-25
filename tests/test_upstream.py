
import os
import re
from lib.control import TestControl 


class TestCtrl(TestControl):

    def setUp(self):

        super().setUp('''
        daemon  off;
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

                add_header X-Upstream 0;

                return  200 "backend1";
            }

            server {
                listen  127.0.0.1:7082;

                add_header X-Upstream 1;

                return  200 "backend2";
            }

            server {
                listen  127.0.0.1:7083;

                add_header X-Upstream 2;

                return  200 "backend3";
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

        self.assertIn(
            'success',
            self.conf(
                {
                    "upstreams": {
                        "one": [
                            {
                                "address": "127.0.0.1:7081",
                            },
                            {
                                "address": "127.0.0.1:7082",
                            },
                        ]
                    },
                }
            ),
            'upstreams initial configuration'
        )

        self.cpu_count = os.cpu_count()

    def get_resps(self, req=100, port=7080):
        resps = [0]
        for _ in range(req):
            headers = self.get(port=port)['headers']
            if 'X-Upstream' in headers:
                ups = int(headers['X-Upstream'])

                if ups > len(resps) - 1:
                    resps.extend([0] * (ups - len(resps) + 1))

                resps[ups] += 1

        return resps

    def get_resps_sc(self, req=100, port=7080):
        to_send = b"""GET / HTTP/1.1
Host: localhost

""" * (
            req - 1
        )

        to_send += b"""GET / HTTP/1.1
Host: localhost
Connection: close

"""

        resp = self.http(to_send, raw_resp=True, raw=True, port=port)
        ups = re.findall('X-Upstream: (\d+)', resp)
        resps = [0] * (int(max(ups)) + 1)

        for i in range(len(ups)):
            resps[int(ups[i])] += 1

        return resps 

    def test_upstreams_rr_no_weight(self):
        resps = self.get_resps()
        self.assertLessEqual(
            abs(resps[0] - resps[1]), self.cpu_count, 'no weight'
        )

        self.assertIn(
            'success',
            self.conf_delete('upstreams/one/0'),
            'no weight server remove',
        )

        resps = self.get_resps(req=50)
        self.assertEqual(resps[1], 50, 'no weight 2')

        self.assertIn(
            'success',
            self.conf({'address': '127.0.0.1:7081'}, 'upstreams/one/0'),
            'no weight server revert',
        )

        self.assertIn(
            'success',
            self.conf({'address': '127.0.0.1:7082'}, 'upstreams/one/1'),
            'no weight server revert',
        )

        resps = self.get_resps()
        self.assertLessEqual(
            abs(resps[0] - resps[1]), self.cpu_count, 'no weight 3'
        )

        self.assertIn(
            'success',
            self.conf({'address': '127.0.0.1:7083'}, 'upstreams/one/2'),
            'no weight server revert',
        )

        resps = self.get_resps()
        self.assertLessEqual(
            max(resps) - min(resps), self.cpu_count, 'no weight 4'
        )

        resps = self.get_resps_sc(req=30)
        self.assertEqual(resps[0], 10, 'no weight 4 0')
        self.assertEqual(resps[1], 10, 'no weight 4 1')
        self.assertEqual(resps[2], 10, 'no weight 4 2')

    def test_upstreams_rr_weight(self):
        self.assertIn(
            'success',
            self.conf('3', 'upstreams/one/0/weight'),
            'configure weight',
        )

        resps = self.get_resps_sc()
        self.assertEqual(resps[0], 75, 'weight 3 0')
        self.assertEqual(resps[1], 25, 'weight 3 1')

        self.assertIn(
            'success',
            self.conf_delete('upstreams/one/0/weight'),
            'configure weight remove',
        )
        resps = self.get_resps_sc(req=10)
        self.assertEqual(resps[0] + resps[1], 10, 'weight 0 0')

        self.assertIn(
            'success',
            self.conf(
                [
                    {"address": "127.0.0.1:7081", "weight": 3},
                    {"address": "127.0.0.1:7083", "weight": 2},
                ],
                'upstreams/one',
            ),
            'configure weight 2',
        )

        resps = self.get_resps_sc()
        self.assertEqual(resps[0], 60, 'weight 2 0')
        self.assertEqual(resps[2], 40, 'weight 2 1')


if __name__ == '__main__':
    TestCtrl.main()
