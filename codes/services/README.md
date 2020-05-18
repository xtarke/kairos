#InfluxProxy with Grafana
------

## Firewall

* https://wiki.debian.org/Uncomplicated%20Firewall%20%28ufw%29

## Databases

```
CREATE USER <username> WITH PASSWORD '<password>' WITH ALL PRIVILEGES
CREATE USER "username" WITH PASSWORD 'password'
CREATE DATABASE "database"
GRANT ALL ON "username" TO "database" 
```

## Ngynx

* https://gist.github.com/xoseperez/e23334910fb45b0424b35c422760cb87

* Install nginx
    ```apt-get install nginx```
    
* Certificates:

```$ openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 3650 -nodes```


```
echo "deb http://ftp.debian.org/debian stretch-backports main contrib" | sudo tee /etc/apt/sources.list.d/backports.list
apt-get install certbot -t stretch-backports
certbot certonly --webroot -w /var/www/html -d <domain_name>
certbot certificates
```
- Autorenew certificates:

```
$ sudo crontab -e

@weekly /usr/bin/certbot renew --quiet --no-self-upgrade -w /var/www/html/
@daily service nginx reload
```
* Copy ```proxy``` to ```/etc/nginx/sites-available/```

* Install Grafana
	- http://docs.grafana.org/installation/debian/
	- ```sudo dpkg -i grafana.deb```
	- ```systemctl enable grafana-server```
	- ```systemctl start grafana-server```

* Copy ```kairos-service ``` to ```/etc/inid.d/kairos```
	- ```update-rc.d kairos defaults```

