
import os
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

            server {
                listen  127.0.0.1:7080;

                location / {
                    root  html;
                }
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
                    "routes": [
                        {
                            "match": {"method": "GET"},
                            "action": {
                                "return": 200,
                                "text": "body"
                            }
                        }
                    ]
                }
            ),
            'routing configure',
        )

        self.assertEqual(
            self.get()['body'], 'body', 'init'
        )

    def route(self, route):
        return self.conf([route], 'routes')

    def route_match(self, match):
        self.assertIn(
            'success',
            self.route(
                {"match": match, "action": {"return": 200, "text": "body"}}
            ),
            'route match configure',
        )

    def route_match_invalid(self, match):
        self.assertIn(
            'error',
            self.route(
                {"match": match, "action": {"return": 200, "text": "body"}}
            ),
            'route match configure invalid',
        )

    def host(self, host, status):
        self.assertEqual(
            self.get(headers={'Host': host, 'Connection': 'close'})[
                'status'
            ],
            status,
            'match host',
        )

    def test_route_match_method_positive(self):
        self.assertEqual(self.get()['status'], 200, 'GET')
        self.assertEqual(self.post()['status'], 404, 'POST')

    def test_routes_match_method_positive_many(self):
        self.route_match({"method": ["GET", "POST"]})

        self.assertEqual(self.get()['status'], 200, 'GET')
        self.assertEqual(self.post()['status'], 200, 'POST')
        self.assertEqual(self.delete()['status'], 405, 'DELETE')

    def test_routes_match_method_negative(self):
        self.route_match({"method": "!GET"})

        self.assertEqual(self.get()['status'], 404, 'GET')
        self.assertEqual(self.post()['status'], 200, 'POST')

    def test_routes_match_method_negative_many(self):
        self.route_match({"method": ["!GET", "!POST"]})

        self.assertEqual(self.get()['status'], 404, 'GET')
        self.assertEqual(self.post()['status'], 404, 'POST')
        self.assertEqual(self.delete()['status'], 200, 'DELETE')

    def test_routes_match_method_wildcard_left(self):
        self.route_match({"method": "*ET"})

        self.assertEqual(self.get()['status'], 200, 'GET')
        self.assertEqual(self.post()['status'], 404, 'POST')

    def test_routes_match_method_wildcard_right(self):
        self.route_match({"method": "GE*"})

        self.assertEqual(self.get()['status'], 200, 'GET')
        self.assertEqual(self.post()['status'], 404, 'POST')

    def test_routes_match_method_wildcard_left_right(self):
        self.route_match({"method": "*GET*"})

        self.assertEqual(self.get()['status'], 200, 'GET')
        self.assertEqual(self.post()['status'], 404, 'POST')

    def test_routes_match_method_wildcard(self):
        self.route_match({"method": "*"})

        self.assertEqual(self.get()['status'], 200, 'GET')

    def test_routes_match_invalid(self):
        self.route_match_invalid({"method": "**"})
        self.route_match_invalid({"method": "blah**"})
        self.route_match_invalid({"host": "*blah*blah"})
        self.route_match_invalid({"host": "blah*blah*blah"})
        self.route_match_invalid({"host": "blah*blah*"})

    def test_routes_match_wildcard_middle(self):
        self.route_match({"host": "ex*le"})

        self.host('example', 200)
        self.host('www.example', 404)
        self.host('example.com', 404)
        self.host('exampl', 404)

    def test_routes_match_method_case_insensitive(self):
        self.route_match({"method": "get"})

        self.assertEqual(self.get()['status'], 200, 'GET')

    def test_routes_match_wildcard_left_case_insensitive(self):
        self.route_match({"method": "*get"})
        self.assertEqual(self.get()['status'], 200, 'GET')

        self.route_match({"method": "*et"})
        self.assertEqual(self.get()['status'], 200, 'GET')

    def test_routes_match_wildcard_middle_case_insensitive(self):
        self.route_match({"method": "g*t"})

        self.assertEqual(self.get()['status'], 200, 'GET')

    def test_routes_match_wildcard_right_case_insensitive(self):
        self.route_match({"method": "get*"})
        self.assertEqual(self.get()['status'], 200, 'GET')

        self.route_match({"method": "ge*"})
        self.assertEqual(self.get()['status'], 200, 'GET')

    def test_routes_match_wildcard_substring_case_insensitive(self):
        self.route_match({"method": "*et*"})

        self.assertEqual(self.get()['status'], 200, 'GET')

    def test_routes_match_wildcard_left_case_sensitive(self):
        self.route_match({"uri": "*blah"})

        self.assertEqual(self.get(url='/blah')['status'], 200, '/blah')
        self.assertEqual(self.get(url='/BLAH')['status'], 404, '/BLAH')

    def test_routes_match_wildcard_middle_case_sensitive(self):
        self.route_match({"uri": "/b*h"})

        self.assertEqual(self.get(url='/blah')['status'], 200, '/blah')
        self.assertEqual(self.get(url='/BLAH')['status'], 404, '/BLAH')

    def test_routes_match_wildcard_right_case_sensitive(self):
        self.route_match({"uri": "/bla*"})

        self.assertEqual(self.get(url='/blah')['status'], 200, '/blah')
        self.assertEqual(self.get(url='/BLAH')['status'], 404, '/BLAH')

    def test_routes_match_wildcard_substring_case_sensitive(self):
        self.route_match({"uri": "*bla*"})

        self.assertEqual(self.get(url='/blah')['status'], 200, '/blah')
        self.assertEqual(self.get(url='/BLAH')['status'], 404, '/BLAH')

    def test_route_empty(self):
        self.assertIn(
            'success', self.conf([], 'routes'), 'routes empty configure'
        )

        self.assertEqual(self.get()['status'], 404, 'route empty')

    def test_routes_route_match_absent(self):
        self.assertIn(
            'success',
            self.conf([{"action": {"return": 200, "text": "body"}}], 'routes'),
            'route match absent configure',
        )

        self.assertEqual(self.get()['status'], 200, 'route match absent')

    def test_routes_match_empty(self):
        self.assertIn(
            'success',
            self.conf([{"match": {}, "action": {"return": 200, "text": "body"}}], 'routes'),
            'route match empty configure',
        )

        self.assertEqual(self.get()['status'], 200, 'route match empty')

    def test_routes_route_action_absent(self):
        self.skip_alerts.append(r'failed to apply new conf')

        self.assertIn(
            'error',
            self.conf([{"match": {"method": "GET"}}], 'routes'),
            'route action absent configure',
        )
    
    def test_routes_route_empty(self):
        self.assertIn(
            'success',
            self.conf([{"match": {"method": "GET"}, "action": {}}], 'routes'),
            'route action empty configure',
        )

        self.assertEqual(self.get()['status'], 404, 'route action empty')

    def test_routes_rules_two(self):
        self.assertIn(
            'success',
            self.conf(
                [
                    {
                        "match": {"method": "GET"},
                        "action": {"return": 200, "text": "body"},
                    },
                    {
                        "match": {"method": "POST"},
                        "action": {"return": 200, "text": "body"}
                    },
                ],
                'routes',
            ),
            'rules two configure',
        )

        self.assertEqual(self.get()['status'], 200, 'rules two match first')
        self.assertEqual(
            self.post(
                headers={
                    'Host': 'localhost',
                    'Content-Type': 'text/html',
                    'Connection': 'close',
                },
                body='X',
            )['status'],
            200,
            'rules two match second',
        )

    def test_routes_match_host_positive(self):
        self.route_match({"host": "localhost"})

        self.assertEqual(self.get()['status'], 200, 'localhost')
        self.host('localhost.', 200)
        self.host('localhost.', 200)
        self.host('.localhost', 404)
        self.host('www.localhost', 404)
        self.host('localhost1', 404)

    def test_routes_match_host_ipv4(self):
        self.route_match({"host": "127.0.0.1"})

        self.host('127.0.0.1', 200)
        self.host('127.0.0.1:7080', 200)

    def test_routes_match_host_ipv6(self):
        self.route_match({"host": "[::1]"})

        self.host('[::1]', 200)
        self.host('[::1]:7080', 200)

    def test_routes_match_host_positive_many(self):
        self.route_match({"host": ["localhost", "example.com"]})

        self.assertEqual(self.get()['status'], 200, 'localhost')
        self.host('example.com', 200)

    def test_routes_match_host_positive_and_negative(self):
        self.route_match({"host": ["*example.com", "!www.example.com"]})

        self.assertEqual(self.get()['status'], 404, 'localhost')
        self.host('example.com', 200)
        self.host('www.example.com', 404)
        self.host('!www.example.com', 200)
    
    def test_routes_match_host_positive_and_negative_wildcard(self):
        self.route_match({"host": ["*example*", "!www.example*"]})

        self.host('example.com', 200)
        self.host('www.example.com', 404)

    def test_routes_match_host_case_insensitive(self):
        self.route_match({"host": "Example.com"})

        self.host('example.com', 200)
        self.host('EXAMPLE.COM', 200)

    def test_routes_match_host_port(self):
        self.route_match({"host": "example.com"})

        self.host('example.com:7080', 200)

    def test_routes_match_uri_positive(self):
        self.route_match({"uri": ["/blah", "/slash/"]})

        self.assertEqual(self.get()['status'], 404, '/')
        self.assertEqual(self.get(url='/blah')['status'], 200, '/blah')
        self.assertEqual(self.get(url='/blah#foo')['status'], 200, '/blah#foo')
        self.assertEqual(self.get(url='/blah?var')['status'], 200, '/blah?var')
        self.assertEqual(self.get(url='//blah')['status'], 200, '//blah')
        self.assertEqual(
            self.get(url='/slash/foo/../')['status'], 200, 'relative'
        )
        self.assertEqual(self.get(url='/slash/./')['status'], 200, '/slash/./')
        self.assertEqual(
            self.get(url='/slash//.//')['status'], 200, 'adjacent slashes'
        )
        self.assertEqual(self.get(url='/%')['status'], 400, 'percent')
        self.assertEqual(self.get(url='/%1')['status'], 400, 'percent digit')
        self.assertEqual(self.get(url='/%A')['status'], 400, 'percent letter')
        self.assertEqual(
            self.get(url='/slash/.?args')['status'], 200, 'dot args'
        )
        self.assertEqual(
            self.get(url='/slash/.#frag')['status'], 200, 'dot frag'
        )
        self.assertEqual(
            self.get(url='/slash/foo/..?args')['status'],
            200,
            'dot dot args',
        )
        self.assertEqual(
            self.get(url='/slash/foo/..#frag')['status'],
            200,
            'dot dot frag',
        )
        self.assertEqual(
            self.get(url='/slash/.')['status'], 200, 'trailing dot'
        )
        self.assertEqual(
            self.get(url='/slash/foo/..')['status'],
            200,
            'trailing dot dot',
        )

    def test_routes_match_uri_case_sensitive(self):
        self.route_match({"uri": "/BLAH"})

        self.assertEqual(self.get(url='/blah')['status'], 404, '/blah')
        self.assertEqual(self.get(url='/BlaH')['status'], 404, '/BlaH')
        self.assertEqual(self.get(url='/BLAH')['status'], 200, '/BLAH')

    def test_routes_match_uri_normalize(self):
        self.route_match({"uri": "/blah"})

        self.assertEqual(
            self.get(url='/%62%6c%61%68')['status'], 200, 'normalize'
        )

    def test_routes_match_empty_array(self):
        self.route_match({"uri": []})

        self.assertEqual(self.get(url='/blah')['status'], 200, 'empty array')

    def test_routes_reconfigure(self):
        self.assertIn('success', self.conf([], 'routes'), 'redefine')
        self.assertEqual(self.get()['status'], 404, 'redefine request')

        self.assertIn(
            'success',
            self.conf([{"action": {"return": 200, "text": "body"}}], 'routes'),
            'redefine 2',
        )
        self.assertEqual(self.get()['status'], 200, 'redefine request 2')

        self.assertIn('success', self.conf([], 'routes'), 'redefine 3')
        self.assertEqual(self.get()['status'], 404, 'redefine request 3')

    def test_routes_edit(self):
        self.route_match({"method": "GET"})

        self.assertEqual(self.get()['status'], 200, 'routes edit GET')
        self.assertEqual(self.post()['status'], 404, 'routes edit POST')

        self.assertIn(
            'success',
            self.conf_post(
                {
                    "match": {"method": "POST"},
                    "action": {"return": 200, "text": "body"},
                },
                'routes',
            ),
            'routes edit configure 2',
        )
        self.assertEqual(
            'GET',
            self.conf_get('routes/0/match/method'),
            'routes edit configure 2 check',
        )
        self.assertEqual(
            'POST',
            self.conf_get('routes/1/match/method'),
            'routes edit configure 2 check 2',
        )

        self.assertEqual(self.get()['status'], 200, 'routes edit GET 2')
        self.assertEqual(self.post()['status'], 200, 'routes edit POST 2')
        
        self.assertIn(
            'success',
            self.conf_delete('routes/0'),
            'routes edit configure 3',
        )

        self.assertEqual(self.get()['status'], 404, 'routes edit GET 3')
        self.assertEqual(self.post()['status'], 200, 'routes edit POST 3')

        self.assertIn(
            'error',
            self.conf_delete('routes/1'),
            'routes edit configure invalid',
        )
        self.assertIn(
            'error',
            self.conf_delete('routes/-1'),
            'routes edit configure invalid 2',
        )
        self.assertIn(
            'error',
            self.conf_delete('routes/blah'),
            'routes edit configure invalid 3',
        )

        self.assertEqual(self.get()['status'], 404, 'routes edit GET 4')
        self.assertEqual(self.post()['status'], 200, 'routes edit POST 4')

        self.assertIn(
            'success',
            self.conf_delete('routes/0'),
            'routes edit configure 5',
        )

        self.assertEqual(self.get()['status'], 404, 'routes edit GET 5')
        self.assertEqual(self.post()['status'], 404, 'routes edit POST 5')

        self.assertIn(
            'success',
            self.conf_post(
                {
                    "match": {"method": "POST"},
                    "action": {"return": 200, "text": "body"},
                },
                'routes',
            ),
            'routes edit configure 6',
        )

        self.assertEqual(self.get()['status'], 404, 'routes edit GET 6')
        self.assertEqual(self.post()['status'], 200, 'routes edit POST 6')

    def test_match_edit(self):
        self.skip_alerts.append(r'failed to apply new conf')

        self.route_match({"method": ["GET", "POST"]})

        self.assertEqual(self.get()['status'], 200, 'match edit GET')
        self.assertEqual(self.post()['status'], 200, 'match edit POST')
        self.assertEqual(self.put()['status'], 405, 'match edit PUT')

        self.assertIn(
            'success',
            self.conf_post('\"PUT\"', 'routes/0/match/method'),
            'match edit configure 2',
        )
        self.assertListEqual(
            ['GET', 'POST', 'PUT'],
            self.conf_get('routes/0/match/method'),
            'match edit configure 2 check',
        )

        self.assertEqual(self.get()['status'], 200, 'match edit GET 2')
        self.assertEqual(self.post()['status'], 200, 'match edit POST 2')
        self.assertEqual(self.put()['status'], 200, 'match edit PUT 2')

        self.assertIn(
            'success',
            self.conf_delete('routes/0/match/method/1'),
            'match edit configure 3',
        )
        self.assertListEqual(
            ['GET', 'PUT'],
            self.conf_get('routes/0/match/method'),
            'match edit configure 3 check',
        )

        self.assertEqual(self.get()['status'], 200, 'match edit GET 3')
        self.assertEqual(self.post()['status'], 404, 'match edit POST 3')
        self.assertEqual(self.put()['status'], 200, 'match edit PUT 3')

        self.assertIn(
            'success',
            self.conf_delete('routes/0/match/method/1'),
            'match edit configure 4',
        )
        self.assertListEqual(
            ['GET'],
            self.conf_get('routes/0/match/method'),
            'match edit configure 4 check',
        )

        self.assertEqual(self.get()['status'], 200, 'match edit GET 4')
        self.assertEqual(self.post()['status'], 404, 'match edit POST 4')
        self.assertEqual(self.put()['status'], 405, 'match edit PUT 4')

        self.assertIn(
            'error',
            self.conf_delete('routes/0/match/method/1'),
            'match edit configure invalid',
        )
        self.assertIn(
            'error',
            self.conf_delete('routes/0/match/method/-1'),
            'match edit configure invalid 2',
        )
        self.assertIn(
            'error',
            self.conf_delete('routes/0/match/method/blah'),
            'match edit configure invalid 3',
        )
        self.assertListEqual(
            ['GET'],
            self.conf_get('routes/0/match/method'),
            'match edit configure 5 check',
        )

        self.assertEqual(self.get()['status'], 200, 'match edit GET 5')
        self.assertEqual(self.post()['status'], 404, 'match edit POST 5')
        self.assertEqual(self.put()['status'], 405, 'match edit PUT 5')

        self.assertEqual(self.get()['status'], 200, 'match edit GET 5')
        self.assertEqual(self.post()['status'], 404, 'match edit POST 5')
        self.assertEqual(self.put()['status'], 405, 'match edit PUT 5')

        self.assertIn(
            'success',
            self.conf_delete('routes/0/match/method/0'),
            'match edit configure 6',
        )
        self.assertListEqual(
            [],
            self.conf_get('routes/0/match/method'),
            'match edit configure 6 check',
        )

        self.assertEqual(self.get()['status'], 200, 'match edit GET 6')
        self.assertEqual(self.post()['status'], 200, 'match edit POST 6')
        self.assertEqual(self.put()['status'], 200, 'match edit PUT 6')

        self.assertIn(
            'success',
            self.conf('"GET"', 'routes/0/match/method'),
            'match edit configure 7',
        )

        self.assertEqual(self.get()['status'], 200, 'match edit GET 7')
        self.assertEqual(self.post()['status'], 404, 'match edit POST 7')
        self.assertEqual(self.put()['status'], 405, 'match edit PUT 7')

        self.assertIn(
            'error',
            self.conf_delete('routes/0/match/method/0'),
            'match edit configure invalid 5',
        )
        self.assertIn(
            'success',
            self.conf({}, 'routes/0/action'),
            'match edit configure 6',
        )

        self.assertIn(
            'success',
            self.conf({}, 'routes/0/match'),
            'match edit configure 8',
        )

        self.assertEqual(self.get()['status'], 404, 'match edit GET 8')

    def test_routes_match_rules(self):
        self.route_match({"method": "GET", "host": "localhost", "uri": "/"})

        self.assertEqual(self.get()['status'], 200, 'routes match rules')

    def test_routes_match_headers(self):
        self.route_match({"headers": {"host": "localhost"}})

        self.assertEqual(self.get()['status'], 200, 'match headers')
        self.host('Localhost', 200)
        self.host('localhost.com', 404)
        self.host('llocalhost', 404)
        self.host('host', 404)

    def test_routes_match_headers_multiple(self):
        self.route_match({"headers": {"host": "localhost", "x-blah": "test"}})

        self.assertEqual(self.get()['status'], 404, 'match headers multiple')
        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "X-blah": "test",
                    "Connection": "close",
                }
            )['status'],
            200,
            'match headers multiple 2',
        )

        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "X-blah": "",
                    "Connection": "close",
                }
            )['status'],
            404,
            'match headers multiple 3',
        )

    def test_routes_match_headers_multiple_values(self):
        self.route_match({"headers": {"x-blah": "test"}})

        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "X-blah": ["test", "test", "test"],
                    "Connection": "close",
                }
            )['status'],
            200,
            'match headers multiple values',
        )
        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "X-blah": ["test", "blah", "test"],
                    "Connection": "close",
                }
            )['status'],
            404,
            'match headers multiple values 2',
        )
        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "X-blah": ["test", "", "test"],
                    "Connection": "close",
                }
            )['status'],
            404,
            'match headers multiple values 3',
        )

    def test_routes_match_headers_multiple_rules(self):
        self.route_match({"headers": {"x-blah": ["test", "blah"]}})

        self.assertEqual(
            self.get()['status'], 404, 'match headers multiple rules'
        )
        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "X-blah": "test",
                    "Connection": "close",
                }
            )['status'],
            200,
            'match headers multiple rules 2',
        )
        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "X-blah": "blah",
                    "Connection": "close",
                }
            )['status'],
            200,
            'match headers multiple rules 3',
        )
        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "X-blah": ["test", "blah", "test"],
                    "Connection": "close",
                }
            )['status'],
            200,
            'match headers multiple rules 4',
        )

        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "X-blah": ["blah", ""],
                    "Connection": "close",
                }
            )['status'],
            404,
            'match headers multiple rules 5',
        )

    def test_routes_match_headers_case_insensitive(self):
        self.route_match({"headers": {"X-BLAH": "TEST"}})

        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "x-blah": "test",
                    "Connection": "close",
                }
            )['status'],
            200,
            'match headers case insensitive',
        )

    def test_routes_match_headers_invalid(self):
        self.route_match_invalid({"headers": ["blah"]})
        self.route_match_invalid({"headers": {"foo": ["bar", {}]}})
        self.route_match_invalid({"headers": {"": "blah"}})

    def test_routes_match_headers_empty_rule(self):
        self.route_match({"headers": {"host": ""}})

        self.assertEqual(self.get()['status'], 404, 'localhost')
        self.host('', 400)

    def test_routes_match_headers_empty(self):
        self.route_match({"headers": {}})
        self.assertEqual(self.get()['status'], 200, 'empty')

        self.route_match({"headers": []})
        self.assertEqual(self.get()['status'], 200, 'empty 2')

    def test_routes_match_headers_rule_array_empty(self):
        self.route_match({"headers": {"blah": []}})

        self.assertEqual(self.get()['status'], 404, 'array empty')
        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "blah": "foo",
                    "Connection": "close",
                }
            )['status'], 200, 'match headers rule array empty 2'
        )

    def test_routes_match_headers_array(self):
        self.route_match(
            {
                "headers": [
                    {"x-header1": "foo*"},
                    {"x-header2": "bar"},
                    {"x-header3": ["foo", "bar"]},
                    {"x-header1": "bar", "x-header4": "foo"},
                ]
            }
        )

        self.assertEqual(self.get()['status'], 404, 'match headers array')
        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "x-header1": "foo123",
                    "Connection": "close",
                }
            )['status'],
            200,
            'match headers array 2',
        )
        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "x-header2": "bar",
                    "Connection": "close",
                }
            )['status'],
            200,
            'match headers array 3',
        )
        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "x-header3": "bar",
                    "Connection": "close",
                }
            )['status'],
            200,
            'match headers array 4',
        )
        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "x-header1": "bar",
                    "Connection": "close",
                }
            )['status'],
            404,
            'match headers array 5',
        )
        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "x-header1": "bar",
                    "x-header4": "foo",
                    "Connection": "close",
                }
            )['status'],
            200,
            'match headers array 6',
        )

        self.assertIn(
            'success',
            self.conf_delete('routes/0/match/headers/1'),
            'match headers array configure 2',
        )

        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "x-header2": "bar",
                    "Connection": "close",
                }
            )['status'],
            404,
            'match headers array 7',
        )
        self.assertEqual(
            self.get(
                headers={
                    "Host": "localhost",
                    "x-header3": "foo",
                    "Connection": "close",
                }
            )['status'],
            200,
            'match headers array 8',
        )

    def test_routes_match_arguments(self):
        self.route_match({"arguments": {"foo": "bar"}})

        self.assertEqual(self.get()['status'], 404, 'args')
        self.assertEqual(self.get(url='/?foo=bar')['status'], 200, 'args 2')
        self.assertEqual(self.get(url='/?foo=bar1')['status'], 404, 'args 3')
        self.assertEqual(self.get(url='/?1foo=bar')['status'], 404, 'args 4')
        self.assertEqual(self.get(url='/?Foo=bar')['status'], 404, 'case')
        self.assertEqual(self.get(url='/?foo=Bar')['status'], 404, 'case 2')

    def test_routes_match_arguments_empty(self):
        self.route_match({"arguments": {}})
        self.assertEqual(self.get()['status'], 200, 'arguments empty')

        self.route_match({"arguments": []})
        self.assertEqual(self.get()['status'], 200, 'arguments empty 2')

    def test_routes_match_arguments_invalid(self):
        self.route_match_invalid({"arguments": ["var"]})
        self.route_match_invalid({"arguments": [{"var1": {}}]})
        self.route_match_invalid({"arguments": {"": "bar"}})

    def test_routes_match_arguments_chars(self):
        self.route_match({"arguments": {"foo": "-._()[],;"}})

        self.assertEqual(self.get(url='/?foo=-._()[],;')['status'], 200, 'chs')

    def test_routes_match_arguments_complex(self):
        self.route_match({"arguments": {"foo": ""}})

        self.assertEqual(self.get(url='/?foo')['status'], 200, 'complex')
        self.assertEqual(
            self.get(url='/?blah=blah&foo=')['status'], 200, 'complex 2'
        )
        self.assertEqual(
            self.get(url='/?&&&foo&&&')['status'], 200, 'complex 3'
        )
        self.assertEqual(
            self.get(url='/?foo&foo=bar&foo')['status'], 404, 'complex 4'
        )
        self.assertEqual(
            self.get(url='/?foo=&foo')['status'], 200, 'complex 5'
        )
        self.assertEqual(
            self.get(url='/?&=&foo&==&')['status'], 200, 'complex 6'
        )
        self.assertEqual(
            self.get(url='/?&=&bar&==&')['status'], 404, 'complex 7'
        )

    def test_routes_match_arguments_multiple(self):
        self.route_match({"arguments": {"foo": "bar", "blah": "test"}})

        self.assertEqual(self.get()['status'], 404, 'multiple')
        self.assertEqual(
            self.get(url='/?foo=bar&blah=test')['status'], 200, 'multiple 2'
        )
        self.assertEqual(
            self.get(url='/?foo=bar&blah')['status'], 404, 'multiple 3'
        )

    def test_routes_match_arguments_multiple_rules(self):
        self.route_match({"arguments": {"foo": ["bar", "blah"]}})

        self.assertEqual(self.get()['status'], 404, 'rules')
        self.assertEqual(self.get(url='/?foo=bar')['status'], 200, 'rules 2')
        self.assertEqual(self.get(url='/?foo=blah')['status'], 200, 'rules 3')
        self.assertEqual(
            self.get(url='/?foo=blah&foo=bar&foo=blah')['status'],
            200,
            'rules 4',
        )
        self.assertEqual(
            self.get(url='/?foo=blah&foo=bar&foo=')['status'], 404, 'rules 5'
        )

    def test_routes_match_arguments_array(self):
        self.route_match(
            {
                "arguments": [
                    {"var1": "val1*"},
                    {"var2": "val2"},
                    {"var3": ["foo", "bar"]},
                    {"var1": "bar", "var4": "foo"},
                ]
            }
        )

        self.assertEqual(self.get()['status'], 404, 'arr')
        self.assertEqual(self.get(url='/?var1=val123')['status'], 200, 'arr 2')
        self.assertEqual(self.get(url='/?var2=val2')['status'], 200, 'arr 3')
        self.assertEqual(self.get(url='/?var3=bar')['status'], 200, 'arr 4')
        self.assertEqual(self.get(url='/?var1=bar')['status'], 404, 'arr 5')
        self.assertEqual(
            self.get(url='/?var1=bar&var4=foo')['status'], 200, 'arr 6'
        )

        self.assertIn(
            'success',
            self.conf_delete('routes/0/match/arguments/1'),
            'match arguments array configure 2',
        )

        self.assertEqual(self.get(url='/?var2=val2')['status'], 404, 'arr 7')
        self.assertEqual(self.get(url='/?var3=foo')['status'], 200, 'arr 8')

    def test_routes_match_scheme(self):
        self.route_match({"scheme": "http"})
        self.route_match({"scheme": "https"})
        self.route_match({"scheme": "HtTp"})
        self.route_match({"scheme": "HtTpS"})

    def test_routes_match_scheme_invalid(self):
        self.route_match_invalid({"scheme": ["http"]})
        self.route_match_invalid({"scheme": "ftp"})
        self.route_match_invalid({"scheme": "ws"})
        self.route_match_invalid({"scheme": "*"})
        self.route_match_invalid({"scheme": ""})


if __name__ == '__main__':
    TestCtrl.main()
