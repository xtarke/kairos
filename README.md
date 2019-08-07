# Projeto Kairos


## Guia de instalação banco de dados temporal

- Instalar InfluxDB
- Instalar Grafana [GrafanaInstall](https://grafana.com/grafana/download?platform=arm)
- Instalar Python3 Influx: ```pip3 install influxdb --user``` 
- Instalar paho-mqtt: ```pip3 install paho-mqtt --user``` 

### Configurar InfluxDB

Maiores detalhes  em [InfluxManual](https://docs.influxdata.com/influxdb/v1.7/administration/authentication_and_authorization/#authorization)

- Criação usuário _admin_: 
```
CREATE USER admin WITH PASSWORD '<password>' WITH ALL PRIVILEGES
```

- Influx desabilita uysuários por padrão, portanto altera a configuração de _/etc/influxdb/influxdb.conf_ para:
``` 
[http]
  enabled = true
  bind-address = ":8086"
  auth-enabled = true #<--------------
  log-enabled = true
  write-tracing = false
  pprof-enabled = false
  https-enabled = true
  https-certificate = "/etc/ssl/influxdb.pem"
```

- Criação usuário _xyz_:
```
    CREATE USER xyz WITH PASSWORD '123456'
```

- Conexão a partir desse momento exige usuário e senha:

```
    influx -username admin -password <senha>
```

- Criação do banco de dados:

```
    CREATE DATABASE "myDB"
```

- Criação do banco de dados com duração de 1 dia e replicação igual a 1 [Database Mangement](https://docs.influxdata.com/influxdb/v1.7/query_language/database_management/):

```
     CREATE RETENTION POLICY "one_day_only" ON "myDB" DURATION 1d REPLICATION 1
```

- Permissão de escrita/leitura para o usuário:

```
    GRANT ALL ON "database_name" TO "username"
```

## Manutenção banco de dados:

```
show database 
use <database_name>
select * from camera_temp
drop series from camera_temp
```
