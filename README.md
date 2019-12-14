NGINX Controller Module
=======================


Contents
========
* [Features](#features)
* [Directives](#directives)
* [Examples](#examples)


Features
========

- display stub status with json format
- display header status with json format


Directives
==========


ctrl_stats_zone
---------------

**syntax:**  *ctrl_stats_zone zone=NAME:SIZE*

**context:** *http*

Creates a shared zone ``NAME`` with the ``SIZE`` for storing statistics data.


ctrl_stats
----------

**syntax:**  *ctrl_stats ZONE_NAME*

**context:** *http,server,location*


ctrl_stats_display
------------------

**syntax:**  *ctrl_stats_display ZONE_NAME*

**context:** *location*


Examples
=========
nginx.conf
```
    events {}

    http {
        ctrl_stats_zone  zone=stats:10M;

        server {
            listen  80;

            location / {
                ctrl_stats  stats;
            }
        }

        server {
            listen  8000;

            location /stats {
                ctrl_stats_display  stats;
            }
        }
    }
```

display all stats

```
curl http://127.0.0.1:8000/stats/
{
    "stub": {
        "active": 2,
        "accepted": 4,
        "handled": 4,
        "requests": 37,
        "reading": 0,
        "writing": 1,
        "waiting": 1
    },

    "status": {
        "n1xx": 0,
        "n2xx": 36,
        "n3xx": 0,
        "n4xx": 0,
        "n5xx": 0,
        "total": 36
    }
}
```

display stats stub

```
curl http://127.0.0.1:8000/stats/stub
{
    "active": 2,
    "accepted": 5,
    "handled": 5,
    "requests": 77,
    "reading": 0,
    "writing": 1,
    "waiting": 1
}
```

display stats status

```
curl http://127.0.0.1:8000/stats/status
{
    "n1xx": 0,
    "n2xx": 91,
    "n3xx": 0,
    "n4xx": 0,
    "n5xx": 0,
    "total": 91
}
```
