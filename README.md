# Projeto Kairos

## Tópicos

### Sensores

- temperatura0: RS485=0x0a
- temperatura1: RS485=0x0e
- temperatura2: RS485=0x0b
- temperatura3: RS485=0x0c
- pressão: 4 a 20mA

### Configuração

- tanque/xyz/cfg/0 : temperatura 0 ativado/desativado
- tanque/xyz/cfg/1 : temperatura 1 ativado/desativado
- tanque/xyz/cfg/2 : temperatura 2 ativado/desativado
- tanque/xyz/cfg/3 : temperatura 3 ativado/desativado
- tanque/xyz/cfg/4 : pressão ativado/desativado
- Configuração do fundo de escala do sensor de pressão:
    - tanque/xyz/cfg/5 : alpha (multiplicado por 1000).
    - tanque/xyz/cfg/6 : beta (multiplicado por 1000).

```
P(i) = alpha * i - beta

alpha = Full_Scale  * 1000
        ----------
           0.016

beta =  Full_Scale * 0.004       Full_Scale * 1000
        ------------------  =    -----------
            0.016                    4

P(i) = 375000*i - 1500   (0..6000 mBAR)
```

### Dados

- tanque/xyz/temperatura/0
- tanque/xyz/temperatura/1
- tanque/xyz/temperatura/2
- tanque/xyz/temperatura/3
- tanque/xyz/pressao

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

- Influx desabilita usuários por padrão, portanto altere a configuração de _/etc/influxdb/influxdb.conf_ para:
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
show databases
use <database_name>
select * from camera_temp
drop series from camera_temp

select * from "temp20-1" where "Float_value" > 1000

#See: https://stackoverflow.com/questions/39685114/delete-points-with-unwanted-field-values-from-influxdb-measurement

# Copy all valid data to a temporary measurement
SELECT * INTO metrics_clean FROM metrics WHERE cpu!=-1

# Drop existing dirty measurement
DROP measurement metrics

# Copy temporary measurement to existing measurement
SELECT * INTO metrics FROM metrics_clean

With timerange

# Copy all valid data to a temporary measurement within timerange
SELECT * INTO metrics_clean FROM metrics WHERE cpu!=-1 and time > '<start_time>' and time '<end_time>';

# Delete existing dirty data within timerange
DELETE FROM metrics WHERE time > '<start_time>' and time '<end_time>';

# Copy temporary measurement to existing measurement
SELECT * INTO metrics FROM metrics_clean

```
