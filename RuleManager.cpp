#include "RuleManager.h"

const QString RuleManager::kRulesFilePath = "./rules/rules.json";
QStringList RuleManager::forbidden_hosts = QStringList();
QStringList RuleManager::forbidden_users = QStringList();
QStringList RuleManager::redirect_hosts = QStringList();

void RuleManager::LoadFromDisk()
{
	QFile rules_file(RuleManager::kRulesFilePath);

	if (!rules_file.exists())
	{
		return;
	}
	else
	{
		rules_file.open(QIODeviceBase::ReadOnly);
	}

	QByteArray file_content = rules_file.readAll();
	rules_file.close();

	auto rules_obj = QJsonDocument::fromJson(file_content).object();

	auto forbidden_hosts = rules_obj.value("forbiddenHosts").toArray();
	auto forbidden_users = rules_obj.value("forbiddenUsers").toArray();
	auto redirect_hosts = rules_obj.value("redirectHosts").toArray();

	for (auto i : forbidden_hosts)
	{
		RuleManager::forbidden_hosts.append(i.toString());
	}

	for (auto i : forbidden_users)
	{
		RuleManager::forbidden_users.append(i.toString());
	}

	for (auto i : redirect_hosts)
	{
		RuleManager::redirect_hosts.append(i.toString());
	}
}

bool RuleManager::isHostForbidden(const QString & host)
{
	return RuleManager::forbidden_hosts.contains(host);
}

bool RuleManager::isUserForbidden(const QString& user)
{
	return RuleManager::forbidden_users.contains(user);
}

bool RuleManager::shouldHostRedirect(const QString& host)
{
	return RuleManager::redirect_hosts.contains(host);
}
