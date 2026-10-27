#!/usr/bin/env python3
import sys
import os
import socket
import argparse

parser = argparse.ArgumentParser()

parser.add_argument('outdir',
                    type=str,
                    nargs='?',
                    default=os.getcwd(),
                    metavar='<OutputDirectory>')

parser.add_argument('-u', '--uri',
                    metavar="<ApplicationUri>",
                    type=str,
                    default="",
                    dest="uri")

parser.add_argument('-k', '--keysize',
                    metavar="<KeySize>",
                    type=int,
                    dest="keysize")

parser.add_argument('-c', '--certificatename',
                     metavar="<CertificateName>",
                     type=str,
                     default="",
                     dest="certificatename")

args = parser.parse_args()

if not os.path.exists(args.outdir):
    sys.exit('ERROR: Directory %s was not found!' % args.outdir)

keysize = 2048
if args.keysize:
    keysize = args.keysize

if args.uri == "":
    args.uri = "urn:open62541.client.application"
    print("No ApplicationUri given for the certificate. Setting to %s" % args.uri)

if args.certificatename == "":
    certificatename = "client"
    print("No Certificate name provided. Setting to %s" % certificatename)
else:
    certificatename = args.certificatename

certsdir = os.path.dirname(os.path.abspath(__file__))

# Упрощенное определение IP адресов без netifaces
def get_ip_address():
    try:
        # Пробуем получить локальный IP
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except:
        return "127.0.0.1"

ip1 = get_ip_address()
ip2 = "127.0.0.1"
hostname = socket.gethostname()

print(f"Using IP addresses: {ip1}, {ip2}")
print(f"Using hostname: {hostname}")

# Устанавливаем переменные окружения
os.environ['IPADDRESS1'] = ip1
os.environ['IPADDRESS2'] = ip2
os.environ['HOSTNAME'] = hostname
os.environ['URI1'] = args.uri

openssl_conf = os.path.join(certsdir, "localhost.cnf")
os.chdir(os.path.abspath(args.outdir))

# Генерация сертификата
result = os.system("""openssl req \
     -config {} \
     -new \
     -nodes \
     -x509 -sha256  \
     -newkey rsa:{} \
     -keyout localhost.key -days 1825 \
     -subj "/C=DE/L=Here/O=open62541/CN=open62541Client@localhost"\
     -out localhost.crt""".format(openssl_conf, keysize))

if result != 0:
    sys.exit("ERROR: Failed to generate certificate")

# Конвертация в DER
os.system("openssl x509 -in localhost.crt -outform der -out %s_cert.der" % certificatename)
os.system("openssl rsa -inform PEM -in localhost.key -outform DER -out %s_key.der" % certificatename)

# Удаление временных файлов
if os.path.exists("localhost.key"):
    os.remove("localhost.key")
if os.path.exists("localhost.crt"):
    os.remove("localhost.crt")

print("Certificates generated in " + args.outdir)
print("Files: %s_cert.der and %s_key.der" % (certificatename, certificatename))