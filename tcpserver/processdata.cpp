#include "processdata.h"

ProcessData::ProcessData(QObject *parent)
    : QObject{parent}
{
    QTimer::singleShot(10, this, [this]() { generateCryptKey(m_cred.login, m_cred.pass); });
    // generateCryptKey(m_cred.login, m_cred.pass);
}

ProcessData::~ProcessData()
{

}

quint16 ProcessData::random()
{
    quint16 start = 1000;
    quint16 end = 9999;
    QRandomGenerator rnd;
    return rnd.global()->bounded(start, end);
}

void ProcessData::generateCryptKey(const QString &login, const QString &pass)
{
    if (login.isEmpty() || pass.isEmpty()) return;
    //добавляем идентификатор для защиты от rainbow-таблиц :)
    const QString PEPPER = "ingc_app_opcua_sPeCiFIc_pepper7sj'kr&";
    //генерация ключа
    m_cred.key = QCryptographicHash::hash((login + "|" + pass + "|" + PEPPER).toUtf8(), QCryptographicHash::Sha256);
    //генерация вектора инициализации (вычисляем MD5 хеш)
    m_cred.iv = QCryptographicHash::hash(m_cred.key, QCryptographicHash::Md5).left(16);
    emit status(QString("[Info] %1, login: %2").arg(Q_FUNC_INFO, m_cred.login));
}

QByteArray ProcessData::encrypt(const QByteArray &crypba, const QByteArray &key, const QByteArray &iv)
{
    if (crypba.isEmpty() || key.isEmpty() || iv.isEmpty()) return QByteArray();

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return QByteArray();

    //симметричное шифрование AES-256-CBC
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                           reinterpret_cast<const unsigned char*>(key.constData()),
                           reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
        emit status(QString("[Error] %1, Failed to initialize encryption").arg(Q_FUNC_INFO));
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    int len;
    int cipherbytes_len = 0;
    QByteArray cipherbytes(crypba.size() + AES_BLOCK_SIZE, 0);

    if (EVP_EncryptUpdate(ctx,
                          reinterpret_cast<unsigned char*>(cipherbytes.data()), &len,
                          reinterpret_cast<const unsigned char*>(crypba.constData()), crypba.size()) != 1) {
        emit status(QString("[Error] %1, Encryption update failed").arg(Q_FUNC_INFO));
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }
    cipherbytes_len = len;

    if (EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(cipherbytes.data()) + len, &len) != 1) {
        emit status(QString("[Error] %1, Encryption finalization failed").arg(Q_FUNC_INFO));
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }
    cipherbytes_len += len;

    EVP_CIPHER_CTX_free(ctx);
    cipherbytes.resize(cipherbytes_len);
    return cipherbytes;
}

QByteArray ProcessData::decrypt(const QByteArray &encrba, const QByteArray &key, const QByteArray &iv)
{
    if (encrba.isEmpty() || key.isEmpty() || iv.isEmpty()) return QByteArray();

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return QByteArray();

    //симметричное шифрование AES-256-CBC
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                           reinterpret_cast<const unsigned char*>(key.constData()),
                           reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        emit status(QString("[Error] %1, Failed to initialize decryption").arg(Q_FUNC_INFO));
        return QByteArray();
    }

    int len;
    int plainbytes_len = 0;
    QByteArray plainbytes(encrba.size() + AES_BLOCK_SIZE, 0);

    if (EVP_DecryptUpdate(ctx,
                          reinterpret_cast<unsigned char*>(plainbytes.data()), &len,
                          reinterpret_cast<const unsigned char*>(encrba.constData()),
                          encrba.size()) != 1) {
        emit status(QString("[Error] %1, Decryption update failed").arg(Q_FUNC_INFO));
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }
    plainbytes_len = len;

    if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(plainbytes.data()) + len, &len) != 1) {
        emit status(QString("[Error] %1, Decryption finalization failed").arg(Q_FUNC_INFO));
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }
    plainbytes_len += len;

    EVP_CIPHER_CTX_free(ctx);
    plainbytes.resize(plainbytes_len);
    return plainbytes;
}

QByteArray ProcessData::compressData(const QByteArray &ba, bool &compressed)
{
    /* Значение сжатия по умолчанию -1 (-1 - сжатие на 6) (0..9, 0 - отсутсвие сжатия)
     * под капотом zlib, MAX размер до 2Gb, если выше то необходимо использовать потоковое сжатие
     * для данных менее 100байт сжатие неэффективно, но т.к. данные всегда >100 байт, сжимаем всегда */
    if (ba.isEmpty()) return QByteArray();

    if (ba.size() > MAX_COMPRESS_SIZE) {
        emit status(QString("[Error] %1, Data size too large for compression zlib, bytes: %2").arg(Q_FUNC_INFO, ba.size()));
        return QByteArray();
    }
    QByteArray zipba = qCompress(ba, 6); //уровень 6
    if (ba.size() < zipba.size()) {
        compressed = false;
        return ba; //для маленьких данных сжатие неэффективно
    }
    compressed = true;
    return zipba;
}

QByteArray ProcessData::uncompressData(const QByteArray &ba)
{
    if (ba.isEmpty()) return QByteArray();

    if (ba.size() < 6 || MAX_COMPRESS_SIZE < ba.size()) { //MIN и MAX размер zlib данных
        emit status(QString("[Error] %1, The data size is not within the zlib compression range, bytes: %2").arg(Q_FUNC_INFO, ba.size()));
        return QByteArray();
    }
    QByteArray uncompressed = qUncompress(ba);
    if (uncompressed.isEmpty()) return ba; //если распаковать не получилось, значит данные не сжаты, возвращаем, что приняли
    return uncompressed;
}

QByteArray ProcessData::createPacket(const QVariantMap &map)
{
    /* 1.сериализация (структура - байты)
     * 2.сжатие
     * 3.шифрование
     * 4.сериализация, упаковка [bodySize:4][flags:1][rezerv:3][cipherbody:bodySize] header - 8byte */
    if (map.isEmpty()) return QByteArray{};

    //1.сериализация
    QByteArray body;
    {
        /* QDataStream платформонезависимый инструмент, формирует поток байт по умолчания BigEndian, не зависим от платформы */
        QDataStream stream(&body, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_9);
        stream << map;
    } //stream разрушен при выходе из функции

    //2.сжатие, шифрование
    bool isCompressed = false;
    QByteArray cipherbody = encrypt(compressData(body, isCompressed), m_cred.key, m_cred.iv);
    if (cipherbody.isEmpty()) {
        emit status(QString("[Error] %1, Encryption failed, map.size: %2").arg(Q_FUNC_INFO, QString::number(map.size())));
        return QByteArray{};
    }
    //3.проверка размера зашифрованного тела (именно оно уходит в bodySize)
    if (cipherbody.size() > static_cast<qsizetype>(MAX_BODY_SIZE)) {
        emit status(QString("[Error] %1, Cipher body too large: %2 bytes (max %3), map.size: %4").arg(Q_FUNC_INFO,
                                                                                                      QString::number(cipherbody.size()),
                                                                                                      QString::number(MAX_BODY_SIZE),
                                                                                                      QString::number(map.size())));
        return QByteArray{};
    }
    //4.упаковка
    QByteArray packet;
    packet.reserve(HEADER_SIZE + cipherbody.size());
    {
        /* QDataStream платформонезависимый инструмент, формирует поток байт по умолчания BigEndian, не зависим от платформы */
        QDataStream stream(&packet, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_9);
        stream << quint32(cipherbody.size());                   // размер тела - 4byte
        stream << quint8(isCompressed ? FLAG_COMPRESSED : 0);   // флаг        - 1byte
        stream << quint8(0);                                    // reserved1   - 1byte
        stream << quint16(0);                                   // reserved2   - 2byte
        stream.writeRawData(cipherbody.constData(), cipherbody.size()); //пишем сырые данные, constData() - указатель на начало
    } //stream разрушен при выходе из функции
    return packet;
}

void ProcessData::readPacket(QTcpSocket *socket, QHash<QTcpSocket*, STWcli> &hSocket, const QByteArray &packet)
{
    /* Транспортный уровень TCP:
     * 1.данные приходят в сокет непрерывным потоком байт
     * 2.разбивка на блоки данных записанного пакета происходит на уровне TCP (размер 1500байт + сервисные данные итого ~1450байт)
     * 3.TCP гарантирует порядок байт, отсутсвие пропусков и дублей!!!
     *
     * Формат пакета: [bodySize:4][flags:1][rezerv:3][cipherbody:bodySize]
     *
     * Логика:
     * 1. накапливаем байты в буфере клиента
     * 2. в цикле вытаскиваем все полные пакеты
     * 3. на каждый пакет: decrypt -> (uncompress) -> deserialize -> emit
     */
    if (!socket || packet.isEmpty()) return;

    auto it = hSocket.find(socket);
    if (it == hSocket.end()) return;
    STWcli &client = it.value();

    client.buffer.abytes.append(packet);

    while (true) {
        //1.читаем заголовок (один раз на пакет)
        if (!client.buffer.headerRead) {
            if (client.buffer.abytes.size() < HEADER_SIZE) return;

            quint32 bodySize = 0;
            quint8 flags = 0;
            quint8 reserved1 = 0;
            quint16 reserved2 = 0;
            {
                QDataStream stream(client.buffer.abytes);
                stream.setVersion(QDataStream::Qt_6_9);
                stream >> bodySize;
                stream >> flags;
                stream >> reserved1;
                stream >> reserved2;
            }
            if (bodySize == 0 || bodySize > MAX_BODY_SIZE) {
                emit status(QString("[Error] %1, %2 Invalid bodySize: %3").arg(Q_FUNC_INFO, socket->objectName(), QString::number(bodySize)));
                socket->disconnectFromHost();
                return;
            }

            client.buffer.bodySize = bodySize;
            client.buffer.bodyCompressed = (flags & FLAG_COMPRESSED);
            client.buffer.headerRead = true;
        }
        //2. ждём полное тело (заголовок + тело)
        if (client.buffer.abytes.size() < HEADER_SIZE + static_cast<qsizetype>(client.buffer.bodySize)) {
            return; //данных пока не хватает — буфер сохраняем
        }
        //3.извлекаем зашифрованное тело, дешифруем
        QByteArray decryptBody = decrypt(client.buffer.abytes.mid(HEADER_SIZE, client.buffer.bodySize), m_cred.key, m_cred.iv);
        if (decryptBody.isEmpty()) {
            emit status(QString("[Error] %1, %2 Decryption failed, bodySize: %3").arg(Q_FUNC_INFO, socket->objectName(), QString::number(client.buffer.bodySize)));
            socket->disconnectFromHost();
            return;
        }
        //4.распаковка (только если флаг выставлен)
        QByteArray body = client.buffer.bodyCompressed ? uncompressData(decryptBody) : decryptBody;
        if (body.isEmpty()) {
            emit status(QString("[Error] %1, %2 Uncompress failed, bodySize: %3").arg(Q_FUNC_INFO, socket->objectName(), QString::number(client.buffer.bodySize)));
            socket->disconnectFromHost();
            return;
        }
        //5.десериализация
        QVariantMap map;
        {
            QDataStream stream(body);
            stream.setVersion(QDataStream::Qt_6_9);
            stream >> map;
        }
        //6.выталкиваем пакет из буфера ДО emit,
        client.buffer.abytes.remove(0, HEADER_SIZE + client.buffer.bodySize);
        client.buffer.bodySize = 0;
        client.buffer.headerRead = false;
        client.buffer.bodyCompressed = false;

        emit readPacketCompleted(socket, map);
        //7.если в буфере уже есть следующий пакет — цикл повторится

        //защита от бесконтрольного роста буфера
        if (client.buffer.abytes.size() > HEADER_SIZE + static_cast<qsizetype>(MAX_BODY_SIZE)) { //8 + 67 108 864 байт
            emit status(QString("[Error] %1, %2 Buffer overflow: %3 bytes").arg(Q_FUNC_INFO, socket->objectName(), QString::number(client.buffer.abytes.size())));
            socket->disconnectFromHost();
            return;
        }
    }
}

qint64 ProcessData::writeSocketWidthControl(QTcpSocket *socket, const QByteArray &data)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return -1;
    }
    const uint MAX_SOCKET_BUFFER = 2 * 1024 * 1024; //2МБ
    const uint WARNING_BUFFER_SIZE = 512 * 1024;    //512Кб

    //1.если буфер для клиента сликшом большой - отключаем от хоста
    if (socket->bytesToWrite() > MAX_SOCKET_BUFFER) {
        emit status(QString("[Error] %1, %2 Buffer too large 2Mb").arg(Q_FUNC_INFO, socket->objectName()));
        socket->disconnectFromHost();
        return -1;
    }
    //2.пишем данные
    qint64 written = socket->write(data);
    if (written < 0) {
        emit status(QString("[Error] %1, %2, Write error: %3").arg(Q_FUNC_INFO, socket->objectName(), socket->errorString()));
        return written;
    }
    //3.логируем если буфер большой
    if (socket->bytesToWrite() > WARNING_BUFFER_SIZE) {
        emit status(QString("[Warning] %1, %2 Buffer size: %3 KB").arg(Q_FUNC_INFO, socket->objectName(), QString::number(socket->bytesToWrite() / 1024)));
    }
    return written;
}

bool ProcessData::authorization(QTcpSocket *socket, QHash <QTcpSocket*, STWcli> &hSocket, const QVariantMap &map)
{
    /* авторизация клиента, при удачной авторизации, выставляем флаг первого скана для записи всех данных и конфигов
     * в процессе выполнения пишем в сокет только измененные данные, контейнер только с измененными данными готовим
     * в классе Engine, вызываем все по таймеру! */
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return false;

    //клиент авторизован
    if (socket->state() == QAbstractSocket::ConnectedState && socket->property("auth").toBool()) return true;

    //авторизация клиента
    const QString login = map.value("CLIENT.LOGIN").toString();
    const QString pass = map.value("CLIENT.PASS").toString();

    if (socket->objectName().isEmpty() && login == m_cred.login && pass == m_cred.pass) {
        /* если у клиента нет имени присваиваем логин + рандом */
        if (map.value("CLIENT.NAME").toString().isEmpty()) {
            socket->setObjectName(login + " " + QString::number(random()));
        } else {
            socket->setObjectName(login + " " + map.value("CLIENT.NAME").toString());
        }

        /* необходимые свойста клиента для работы */
        socket->setProperty("auth", true);      //удачная авторизация
        socket->setProperty("firstscan", true); //флаг необходимости первого сканирования
        socket->setProperty("tags.history", QVariant{}); //лист с тегами для запроса исторических данных

        auto it = hSocket.find(socket);
        if (it != hSocket.end()) {
            STWcli &client = it.value();
            client.OSver = map.value("CLIENT.OSVER").toString();
            client.appver = map.value("CLIENT.APPVER").toString();
            client.connectDateTime = QDateTime::currentDateTime();
        };

        emit status(QString("[Info] %1, %2 auth: %3").arg(Q_FUNC_INFO, socket->objectName(), QString::number(socket->property("auth").toBool())));
        return true;
    } else {
        /* Ключи шифрования генерируются в зависимости от логина и пароля, если креды не совпадают, расшифровка неудачна
         * коннект скидывается в методе readPacket(), если тело расшифрованного пакета пустое! */
        socket->disconnectFromHost();
    }
    return false;
}

bool ProcessData::authorization(QTcpSocket *socket)
{
    if (!socket) return false;
    return socket->state() == QAbstractSocket::ConnectedState && socket->property("auth").toBool();
}

bool ProcessData::isfirstscan(QTcpSocket *socket, const QVariantMap &map, qint64 &bytes)
{
    /* метод первого сканирования (запись конфигурации впервые подключенным клиентам)
     * флаг необходимости первого сканирования пишем после удачной авторизации клиента
     * в методе authorization! */
    if (!socket || map.isEmpty()) return false;
    if (socket->property("firstscan").toBool()) {
        //пишем весь контейнер с данными при первом скане
        bytes = writeSocketWidthControl(socket, createPacket(map)); //количество байт принятых немедленно буфером сокета (-1 - ошибка, 0 - буфер переполнен)
        socket->setProperty("firstscan", false); //меняем флаг после выполнения первого скана
        emit status(QString("[Info] %1, %2, map.size: %3, bytes: %4").arg(Q_FUNC_INFO,
                                                                          socket->objectName(),
                                                                          QString::number(map.size()),
                                                                          QString::number(bytes)));
        return true;
    }
    return false;
}

qint64 ProcessData::firstscan(QTcpSocket *socket, const QVariantMap &map)
{
    /* метод первого сканирования (запись конфигурации впервые подключенным клиентам)
     * флаг необходимости первого сканирования пишем после удачной авторизации клиента
     * в методе authorization! */
    qint64 bytes = 0;
    if (!socket || map.isEmpty()) return bytes;
    if (socket->property("firstscan").toBool()) {
        //пишем весь контейнер с данными при первом скане
        bytes = writeSocketWidthControl(socket, createPacket(map)); //количество байт принятых немедленно буфером сокета (-1 - ошибка, 0 - буфер переполнен)
        socket->setProperty("firstscan", false); //меняем флаг после выполнения первого скана
        emit status(QString("[Info] %1, %2, map.size: %3, bytes: %4").arg(Q_FUNC_INFO,
                                                                          socket->objectName(),
                                                                          QString::number(map.size()),
                                                                          QString::number(bytes)));
    }
    return bytes;
}

qint64 ProcessData::sendHistoryData(QTcpSocket *socket, const QVariantMap &map)
{
    qint64 bytes = 0;
    if (!socket || map.isEmpty()) return bytes;
    if (socket->property("tags.history").isValid() && socket->property("tags.history") == map.value("TAG.HISTORY")) {
        //пишем весь контейнер с данными при первом скане
        bytes = writeSocketWidthControl(socket, createPacket(map)); //количество байт принятых немедленно буфером сокета (-1 - ошибка, 0 - буфер переполнен)
        socket->setProperty("tags.history", QVariant{}); //меняем флаг после выполнения первого скана
        emit status(QString("[Info] %1, %2, map.size: %3, bytes: %4").arg(Q_FUNC_INFO,
                                                                          socket->objectName(),
                                                                          QString::number(map.size()),
                                                                          QString::number(bytes)));
    }
    return bytes;
}
