# Handle publisher confirms asynchronously #

## Topology ##

`producer` offers the main function.

`msgd` polls on the internal PIPE, the PULL socket to request message publishing (via signal) and the internally used RabbitMQ-C TCP socket.

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

