-include setting.mk
HTTPD_ROOT := httpd
CGI_ROOT := cgi
SOCK_ROOT := sock

all: build install

build:
	make -C $(HTTPD_ROOT) build
	make -C $(CGI_ROOT) build
	make -C $(SOCK_ROOT) build

clean:
	make -C $(HTTPD_ROOT) clean
	make -C $(CGI_ROOT) clean
	make -C $(SOCK_ROOT) clean

install:
	mkdir -p $(HTTPD_DOC_ROOT)
	cp -f spec/form_get2.htm $(HTTPD_DOC_ROOT)
	ls -l $(HTTPD_DOC_ROOT)
	make -C $(CGI_ROOT) install

uninstall:
	make -C $(CGI_ROOT) uninstall

runserver:
	make -C $(HTTPD_ROOT) runserver

.PHONY: all build clean install uninstall runserver
