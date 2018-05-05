#!/usr/bin/env python

import sys
import array
import httplib

prev_val = 'A'

def send(vals):
    global prev_val

    if vals != prev_val:
        print vals
        connection = httplib.HTTPConnection('192.168.1.222')
        connection.request("GET", "/api/rgb?apikey=CB22BE3289153285&value=" + vals)
        connection.sock.settimeout(0.1)

        try:
            response = connection.getresponse()
            response.read()
            print response.status, response.reason
        except Exception as e:
            print e

        connection.close()
        prev_val = vals


while True:
    try:
        send(sys.stdin.readline()[:-1])
    except Exception as e:
        print(e)
