# SAMWISE #

## Overview ##

This repository contains several different subprojects:

- **/client:** Contains client(s) that can communicate with running samwise instances
- **/poc** Proof of concept playground. Used to try out different things before incorporating them into samwise
- **/samwise** The main project containing everything necessary to build samwise.

## Notes ##

* capture local amqp network traffic with tshark: `sudo tshark -f "tcp port 5672" -R amqp -i lo`

