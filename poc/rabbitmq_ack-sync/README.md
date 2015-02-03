# Simple prod/cons with confirm.select enabled #

To have a nice updated overview over the state of the broker:

`watch 'sudo rabbitmqctl list_channels name number user confirm'`

`watch 'sudo rabbitmqctl list_queues name messages_ready'`

To sniff on the network:

`sudo tshark -f "tcp port 5672" -i lo`

