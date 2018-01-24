SERVER_ROOT := ~/public_html

all: build

build:
	make -C cgi
	make -C http

clean:
	make clean -C cgi
	make clean -C http

install-spec:
	make -C spec all
	cp -f spec/cgi/* $(SERVER_ROOT)
	cp -f spec/form_get.htm $(SERVER_ROOT)
	cp -f testdata/* $(SERVER_ROOT)/
	dos2unix $(SERVER_ROOT)/*.txt
	chmod a+rx $(SERVER_ROOT)/*

uninstall-spec:
	$(RM) $(SERVER_ROOT)/*.cgi
	$(RM) $(SERVER_ROOT)/form_get.html
	$(RM) $(SERVER_ROOT)/*.txt

cgi-env-ubuntu-nginx:
	-`dpkg -S fcgiwrap > /dev/null 2>&1`; if [ $$? -eq 1 ]; then sudo apt-get install fcgiwrap -yq; fi
	if [ -d /etc/nginx/conf.d ]; then sudo ln -fs conf/nginx /etc/nginx/conf.d/np-server.conf; fi

cgi-env-ubuntu-apache2:
	sudo apt-get install -y apache2
	sudo a2enmod cgid userdir
	sudo cp -f conf/apache2-userdir.conf /etc/apache2/mods-available/myuserdir.conf
	sudo ln -fs /etc/apache2/mods-available/myuserdir.conf /etc/apache2/mods-enabled/userdir.conf
	sudo service apache2 restart

.PHONY: all clean install uninstall

