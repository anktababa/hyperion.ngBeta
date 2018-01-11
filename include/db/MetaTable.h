#pragma once

// hyperion
#include <db/DBManager.h>

// qt
#include <QDateTime>
#include <QUuid>
#include <QNetworkInterface>
#include <QCryptographicHash>

///
/// @brief meta table specific database interface
///
class MetaTable : public DBManager
{

public:
	/// construct wrapper with plugins table and columns
	MetaTable()
	{
		setTable("meta");

		createTable(QStringList()<<"uuid TEXT"<<"created_at TEXT"<<"last_rep TEXT"<<"friendly_name TEXT");
	};
	~MetaTable(){};

	///
	/// @brief Get the uuid, if the uuid is not set it will be created
	/// @return The uuid
	///
	inline const QString getUUID()
	{
		QVector<QVariantMap> results;
		getRecords(results, QStringList() << "uuid");

		for(const auto & entry : results)
		{
			if(!entry["uuid"].toString().isEmpty())
				return entry["uuid"].toString();
		}

		// create new uuidv5 based on net adapter MAC, save to db and return
		QString hash;
		foreach(QNetworkInterface interface, QNetworkInterface::allInterfaces())
		{
			if (!(interface.flags() & QNetworkInterface::IsLoopBack))
			{
				hash = QCryptographicHash::hash(interface.hardwareAddress().toLocal8Bit(),QCryptographicHash::Sha1).toHex();
				break;
			}
		}
		const QString newUuid = QUuid::createUuidV5(QUuid(), hash).toString().mid(1, 36);
		VectorPair cond;
		cond.append(CPair("uuid",newUuid));
		QVariantMap map;
		map["created_at"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
		createRecord(cond, map);

		return newUuid;
	}

	///
	/// @brief Check if update required
	///
	inline const bool updateRequired(const QString& uuid)
	{
		QVariantMap results;
		VectorPair cond;
		cond.append(CPair("uuid", uuid));
		getRecord(cond, results, QStringList("last_rep"));

		const QString date = results["last_rep"].toString();
		if(!date.isEmpty())
		{
			// compare
			QDateTime newDate = QDateTime::currentDateTimeUtc();
			QDateTime oldDate = QDateTime::fromString(date, Qt::ISODate);
			return (oldDate.daysTo(newDate) >= 7);
		}
		return true;
	}

	///
	/// @brief Update 'last_rep' timestamp
	/// @param uuid  The uuid
	///
	inline void setTimestamp(const QString& uuid)
	{
		VectorPair cond;
		cond.append(CPair("uuid", uuid));
		QVariantMap map;
		map["last_rep"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
		createRecord(cond, map);
	}
};
