make

# copy bin
cp -f ns-serial-mux /usr/bin/ns-serial-mux

# copy conf

cp -f ns-serial-mux.conf /etc/ns-serial-mux.conf

make clean

# copy service file
cp -f debian/ns-serial-mux.service /etc/systemd/system/ns-serial-mux.service

sudo systemctl daemon-reload
systemctl start ns-serial-mux.service
systemctl status ns-serial-mux.service