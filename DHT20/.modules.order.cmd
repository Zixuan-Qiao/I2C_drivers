cmd_/home/debian/nfsmnt/DHT20/modules.order := {   echo /home/debian/nfsmnt/DHT20/DHT20_char.ko; :; } | awk '!x[$$0]++' - > /home/debian/nfsmnt/DHT20/modules.order
