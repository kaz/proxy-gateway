CC := gcc
CFLAGS := -Ofast

proxy-gw:

install:
	cp proxy-gw /usr/sbin
	cp proxy-gw.service /etc/systemd/system
	systemctl daemon-reload
	systemctl enable proxy-gw
	systemctl start proxy-gw

route:
	iptables -t nat -F
	iptables -t nat -A PREROUTING -i eth0 -p tcp --dport  80 -j REDIRECT --to-port 14514
	iptables -t nat -A PREROUTING -i eth0 -p tcp --dport 443 -j REDIRECT --to-port 14514
