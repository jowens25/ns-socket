make

# copy bin
cp -f ns-socket-mux /usr/bin/ns-socket-mux

# copy conf

cp -f ns-socket-mux.conf /etc/ns-socket-mux.conf

make clean

# copy service file
cp -f debian/ns-socket-mux.service /etc/systemd/system/ns-socket-mux.service

sudo systemctl daemon-reload
systemctl start ns-socket-mux.service
systemctl status ns-socket-mux.service