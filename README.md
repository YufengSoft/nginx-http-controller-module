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

**syntax:**  *ctrl_stats on|off*

**default:**  *ctrl_stats_display off*

**context:** *http,server,location*

ctrl_stats_display
------------------

**syntax:**  *ctrl_stats_display*

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
                ctrl_stats  on;
            }
        }

        server {
            listen  8000;

            location /stats {
                ctrl_stats_display;
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

##  Feedback
Feel free to use this module, don't hesitate to tell me more about what you want to add.

Enjoy.

##  Authors
Zhidao Hong(洪志道) `hongzhidao at gmail dot com`  

## Copyright & License
I borrowed a lot of code from nginx/unit for basic infrastructure. This part of code is copyrighted by Igor Sysoev. 

This module is licensed under the BSD license.  
```bash
    Copyright (C) 2019 by Zhidao Hong <hongzhidao@gmail.com>.
    
    All rights reserved.
    
    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are
    met:
    
    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
    
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
    IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
    TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
    TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

