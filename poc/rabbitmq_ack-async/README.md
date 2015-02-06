# Handle publisher confirms asynchronously #

## Introduction ##
This proof of concept shows how to publish messages and send synchronous AMQP methods while asynchronously handle acknowlegdments from a channel that is in confirm mode. The RabbitMQ-C library internally buffers incoming frames. When a RPC call is made, outstanding acknowledgements are buffered by the library and consumed after the RPC reply frame arrived. In this example the producer constantly sends messages and tries to declare an exchange per every 100 messages sent.

Usage: `./producer host port n`

The parameters `host` and `port` define where the broker can be reached (e.g. localhost 5672) and `n` is the number of messages to be sent.

## Topology ##

`producer` offers the main function.

`msgd` polls on the internal PIPE, the PULL socket to request message publishing and the internally used RabbitMQ-C TCP socket.

`rabbit` abstracts the RabbitMQ-C library calls.


```

"o"      = bind
"[^v<>]" = connect

           BROKER
             ^
             |
        consume/publish
             | 
             o rabbit_new (amqp_connection_state)
           msgd 
          /  o PULL
         /   |
       PIPE  |
         \   |
          \  v PUSH
         producer


```

## Some notes ##

To have a nice updated overview over the state of the broker:

`watch 'sudo rabbitmqctl list_channels name number user confirm'`

`watch 'sudo rabbitmqctl list_queues name messages_ready'`

To sniff on the network:

`sudo tshark -f "tcp port 5672" -i lo`

