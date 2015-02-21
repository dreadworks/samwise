# SAMWISE #

## Overview ##

This repository contains several different subprojects:

- **/client:** Contains client(s) that can communicate with running samwise instances
- **/poc** Proof of concept playground. Used to try out different things before incorporating them into samwise
- **/samwise** The main project containing everything necessary to build samwise
- **/rfc** Place to write down the most important specifications

## Notes ##

* create [heap profiles](http://valgrind.org/docs/manual/ms-manual.html) `valgrind --tool=massif --time-unit=B PROG` then read them with `ms_print`
* capture local amqp network traffic with tshark: `sudo tshark -f "tcp port 5672" -R amqp -i lo`

