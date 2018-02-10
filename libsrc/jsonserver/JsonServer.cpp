// system includes
#include <stdexcept>

// project includes
#include <jsonserver/JsonServer.h>
#include "JsonClientConnection.h"

// hyperion include
#include <bonjour/bonjourserviceregister.h>
#include <utils/NetOrigin.h>

// qt includes
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QByteArray>

JsonServer::JsonServer(const QJsonDocument& config)
	: QObject()
	, _server(new QTcpServer(this))
	, _openConnections()
	, _log(Logger::getInstance("JSONSERVER"))
	, _netOrigin(NetOrigin::getInstance())
{
	// Set trigger for incoming connections
	connect(_server, SIGNAL(newConnection()), this, SLOT(newConnection()));

	// init
	handleSettingsUpdate(settings::JSONSERVER, config);
}

JsonServer::~JsonServer()
{
	foreach (JsonClientConnection * connection, _openConnections) {
		delete connection;
	}
}

void JsonServer::start()
{
	if(_server->isListening())
		return;

	if (!_server->listen(QHostAddress::Any, _port))
	{
		Error(_log,"Could not bind to port '%d', please use an available port", _port);
		return;
	}
	Info(_log, "Started on port %d", _port);

	if(_serviceRegister == nullptr)
	{
		_serviceRegister = new BonjourServiceRegister(this);
		_serviceRegister->registerService("_hyperiond-json._tcp", _port);
	}
	else if( _serviceRegister->getPort() != _port)
	{
		delete _serviceRegister;
		_serviceRegister = new BonjourServiceRegister(this);
		_serviceRegister->registerService("_hyperiond-json._tcp", _port);
	}
}

void JsonServer::stop()
{
	if(!_server->isListening())
		return;

	_server->close();
	Info(_log, "Stopped");
}

void JsonServer::handleSettingsUpdate(const settings::type& type, const QJsonDocument& config)
{
	if(type == settings::JSONSERVER)
	{
		QJsonObject obj = config.object();
		if(_port != obj["port"].toInt())
		{
			_port = obj["port"].toInt();
			stop();
			start();
		}
	}
}

uint16_t JsonServer::getPort() const
{
	return _port;
}

void JsonServer::newConnection()
{
	while(_server->hasPendingConnections())
	{
		if (QTcpSocket * socket = _server->nextPendingConnection())
		{
			if(_netOrigin->accessAllowed(socket->peerAddress(), socket->localAddress()))
			{
				Debug(_log, "New connection from: %s ",socket->localAddress().toString().toStdString().c_str());
				JsonClientConnection * connection = new JsonClientConnection(socket, _netOrigin->isLocalAddress(socket->peerAddress(), socket->localAddress()));
				_openConnections.insert(connection);

				// register slot for cleaning up after the connection closed
				connect(connection, &JsonClientConnection::connectionClosed, this, &JsonServer::closedConnection);
			}
			else
			{
				socket->close();
			}
		}
	}
}

void JsonServer::closedConnection(void)
{
	JsonClientConnection* connection = qobject_cast<JsonClientConnection*>(sender());
	Debug(_log, "Connection closed");
	_openConnections.remove(connection);

	// schedule to delete the connection object
	connection->deleteLater();
}

void JsonServer::sendMessage(const QJsonObject & message, QTcpSocket * socket)
{
	// serialize message
	QJsonDocument writer(message);
	QByteArray serializedMessage = writer.toJson(QJsonDocument::Compact) + "\n";

	// write message
	socket->write(serializedMessage);
	if (!socket->waitForBytesWritten())
	{
		Debug(_log, "Error while writing data to host");
		return;
	}

	// read reply data
	QByteArray serializedReply;
	while (!serializedReply.contains('\n'))
	{
		// receive reply
		if (!socket->waitForReadyRead())
		{
			Debug(_log, "Error while writing data from host");
			return;
		}

		serializedReply += socket->readAll();
	}

	// parse reply data
	QJsonParseError error;
	QJsonDocument reply = QJsonDocument::fromJson(serializedReply ,&error);

	if (error.error != QJsonParseError::NoError)
	{
		Error(_log, "Error while parsing reply: invalid json");
		return;
	}

}
