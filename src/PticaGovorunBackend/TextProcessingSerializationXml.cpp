#include <memory>
#include <unordered_set>
#include <QFile>
#include <QString>
#include <QDomDocument>
#include <QStringList>
#include <QTextStream>
#include <QXmlStreamWriter>
#include "TextProcessing.h"

namespace PticaGovorun
{
	namespace details
	{
		boost::optional<PartOfSpeech> parseWordClass(QStringRef text)
		{
			struct WordClassToStr
			{
				PartOfSpeech WordClass;
				const wchar_t* CStr;
			};
			static std::array<WordClassToStr, 13> classes = {
				WordClassToStr{ PartOfSpeech::Noun, L"noun" },
				WordClassToStr{ PartOfSpeech::Pronoun, L"pronoun" },
				WordClassToStr{ PartOfSpeech::Preposition, L"preposition" },
				WordClassToStr{ PartOfSpeech::Verb, L"verb" },
				WordClassToStr{ PartOfSpeech::Adverb, L"adverb" },
				WordClassToStr{ PartOfSpeech::VerbalAdverb, L"verbalAdverb" },
				WordClassToStr{ PartOfSpeech::Adjective, L"adjective" },
				WordClassToStr{ PartOfSpeech::Participle, L"participle" },
				WordClassToStr{ PartOfSpeech::Numeral, L"numeral" },
				WordClassToStr{ PartOfSpeech::Conjunction, L"conjunction" },
				WordClassToStr{ PartOfSpeech::Interjection, L"interjection" },
				WordClassToStr{ PartOfSpeech::Particle, L"particle" },
				WordClassToStr{ PartOfSpeech::Irremovable, L"irremovable" },
			};
			for(const auto& pair : classes)
			{
				const QString wordClassStr = QString::fromWCharArray(pair.CStr);
				if (text.compare(wordClassStr, Qt::CaseSensitive) == 0)
					return pair.WordClass;
			}
			return boost::none;
		}

		boost::optional<WordGender> parseWordGender(QStringRef text)
		{
			struct WordGenderToStr
			{
				WordGender Gender;
				const wchar_t* CStr;
			};
			static std::array<WordGenderToStr, 3> classes = {
				WordGenderToStr{ WordGender::Masculine, L"masculine" },
				WordGenderToStr{ WordGender::Feminine, L"feminine" },
				WordGenderToStr{ WordGender::Neuter, L"neuter" },
			};
			for(const auto& pair : classes)
			{
				const QString wordClassStr = QString::fromWCharArray(pair.CStr);
				if (text.compare(wordClassStr, Qt::CaseSensitive) == 0)
					return pair.Gender;
			}
			return boost::none;
		}
		boost::optional<EntityMultiplicity> parseWordNumber(QStringRef text)
		{
			struct WordNumberToStr
			{
				EntityMultiplicity WordNumber;
				const wchar_t* CStr;
			};
			static std::array<WordNumberToStr, 2> classes = {
				WordNumberToStr{ EntityMultiplicity::Singular, L"singular" },
				WordNumberToStr{ EntityMultiplicity::Plural, L"plural" },
			};
			for(const auto& pair : classes)
			{
				const QString wordClassStr = QString::fromWCharArray(pair.CStr);
				if (text.compare(wordClassStr, Qt::CaseSensitive) == 0)
					return pair.WordNumber;
			}
			return boost::none;
		}
		boost::optional<bool> parseBool(QStringRef text)
		{
			struct BoolToStr
			{
				bool Bool;
				const wchar_t* CStr;
			};
			static std::array<BoolToStr, 2> classes = {
				BoolToStr{ false, L"false" },
				BoolToStr{ true, L"true" },
			};
			for(const auto& pair : classes)
			{
				const QString wordClassStr = QString::fromWCharArray(pair.CStr);
				if (text.compare(wordClassStr, Qt::CaseSensitive) == 0)
					return pair.Bool;
			}
			return boost::none;
		}
		boost::optional<WordPerson> parseWordPerson(QStringRef text)
		{
			struct WordPersonToStr
			{
				WordPerson Person;
				const wchar_t* CStr;
			};
			static std::array<WordPersonToStr, 9> classes = {
				WordPersonToStr{ WordPerson::Impersonal, L"impersonal" },
				WordPersonToStr{ WordPerson::I, L"i" },
				WordPersonToStr{ WordPerson::YouFamiliar, L"youFamiliar" },
				WordPersonToStr{ WordPerson::He, L"he" },
				WordPersonToStr{ WordPerson::She, L"she" },
				WordPersonToStr{ WordPerson::It, L"it" },
				WordPersonToStr{ WordPerson::We, L"we" },
				WordPersonToStr{ WordPerson::YouRespectful, L"youRespectful" },
				WordPersonToStr{ WordPerson::They, L"they" },
			};
			for(const auto& pair : classes)
			{
				const QString wordClassStr = QString::fromWCharArray(pair.CStr);
				if (text.compare(wordClassStr, Qt::CaseSensitive) == 0)
					return pair.Person;
			}
			return boost::none;
		}
		boost::optional<WordPerfectiveAspect> parseWordPerfectiveAspect(QStringRef text)
		{
			struct PerfToStr
			{
				WordPerfectiveAspect Perf;
				const wchar_t* CStr;
			};
			static std::array<PerfToStr, 2> classes = {
				PerfToStr{ WordPerfectiveAspect::Imperfective, L"impersonal" },
				PerfToStr{ WordPerfectiveAspect::Perfective, L"perfective" },
			};
			for(const auto& pair : classes)
			{
				const QString wordClassStr = QString::fromWCharArray(pair.CStr);
				if (text.compare(wordClassStr, Qt::CaseSensitive) == 0)
					return pair.Perf;
			}
			return boost::none;
		}
		boost::optional<WordTransitive> parseWordTransitive(QStringRef text)
		{
			struct TransitiveToStr
			{
				WordTransitive Transitive;
				const wchar_t* CStr;
			};
			static std::array<TransitiveToStr, 2> classes = {
				TransitiveToStr{ WordTransitive::Transitive, L"transitive" },
				TransitiveToStr{ WordTransitive::Intransitive, L"intransitive" },
			};
			for(const auto& pair : classes)
			{
				const QString wordClassStr = QString::fromWCharArray(pair.CStr);
				if (text.compare(wordClassStr, Qt::CaseSensitive) == 0)
					return pair.Transitive;
			}
			return boost::none;
		}
		boost::optional<WordCase> parseWordCase(QStringRef text)
		{
			struct CaseToStr
			{
				WordCase Case;
				const wchar_t* CStr;
			};
			static std::array<CaseToStr, 9> classes = {
				CaseToStr{ WordCase::Nominative, L"nominative" },
				CaseToStr{ WordCase::Genitive, L"genitive" },
				CaseToStr{ WordCase::Dative, L"dative" },
				CaseToStr{ WordCase::Acusative, L"acusative" },
				CaseToStr{ WordCase::Instrumental, L"instrumental" },
				CaseToStr{ WordCase::Locative, L"locative" },
				CaseToStr{ WordCase::Vocative, L"vocative" },
			};
			for(const auto& pair : classes)
			{
				const QString wordClassStr = QString::fromWCharArray(pair.CStr);
				if (text.compare(wordClassStr, Qt::CaseSensitive) == 0)
					return pair.Case;
			}
			return boost::none;
		}
		boost::optional<ActionTense> parseWordTense(QStringRef text)
		{
			struct TenseToStr
			{
				ActionTense Tense;
				const wchar_t* CStr;
			};
			static std::array<TenseToStr, 3> classes = {
				TenseToStr{ ActionTense::Past, L"past" },
				TenseToStr{ ActionTense::Present, L"present" },
				TenseToStr{ ActionTense::Future, L"future" },
			};
			for(const auto& pair : classes)
			{
				const QString wordClassStr = QString::fromWCharArray(pair.CStr);
				if (text.compare(wordClassStr, Qt::CaseSensitive) == 0)
					return pair.Tense;
			}
			return boost::none;
		}
		boost::optional<WordDegree> parseWordDegree(QStringRef text)
		{
			struct DegreeToStr
			{
				WordDegree Degree;
				const wchar_t* CStr;
			};
			static std::array<DegreeToStr, 3> classes = {
				DegreeToStr{ WordDegree::Positive, L"positive" },
				DegreeToStr{ WordDegree::Comparative, L"comparative" },
				DegreeToStr{ WordDegree::Superlative, L"superlative" },
			};
			for(const auto& pair : classes)
			{
				const QString wordClassStr = QString::fromWCharArray(pair.CStr);
				if (text.compare(wordClassStr, Qt::CaseSensitive) == 0)
					return pair.Degree;
			}
			return boost::none;
		}
		boost::optional<WordActiveOrPassive> parseWordActiveOrPassive(QStringRef text)
		{
			struct ActiveToStr
			{
				WordActiveOrPassive Active;
				const wchar_t* CStr;
			};
			static std::array<ActiveToStr, 3> classes = {
				ActiveToStr{ WordActiveOrPassive::Active, L"active" },
				ActiveToStr{ WordActiveOrPassive::Passive, L"passive" },
			};
			for(const auto& pair : classes)
			{
				const QString wordClassStr = QString::fromWCharArray(pair.CStr);
				if (text.compare(wordClassStr, Qt::CaseSensitive) == 0)
					return pair.Active;
			}
			return boost::none;
		}
		
		void parseDeclensionDictWordGroup(QXmlStreamReader& xml, WordDeclensionGroup* owningWordGroup)
		{
			while (!xml.atEnd())
			{
				xml.readNext();
				if (xml.isEndElement() && xml.name() == "group")
					break;

				if (xml.isStartElement() && xml.name() == "word")
				{
					WordDeclensionForm wordForm;
					wordForm.OwningWordGroup = owningWordGroup;

					const QXmlStreamAttributes& attrs = xml.attributes();
					if (attrs.hasAttribute("name"))
					{
						QStringRef nameStr = attrs.value("name");
						QString nameQ = QString::fromRawData(nameStr.constData(), nameStr.size());
						wordForm.Name = nameQ.toStdWString();
					}
					if (attrs.hasAttribute("class"))
					{
						QStringRef attrStr = attrs.value("class");
						wordForm.WordClass = details::parseWordClass(attrStr);
					}
					if (attrs.hasAttribute("isInfinitive"))
					{
						QStringRef attrStr = attrs.value("isInfinitive");
						wordForm.IsInfinitive = details::parseBool(attrStr);
					}
					if (attrs.hasAttribute("person"))
					{
						QStringRef attrStr = attrs.value("person");
						wordForm.Person = details::parseWordPerson(attrStr);
					}
					if (attrs.hasAttribute("case"))
					{
						QStringRef attrStr = attrs.value("case");
						wordForm.Case = details::parseWordCase(attrStr);
					}
					if (attrs.hasAttribute("gender"))
					{
						QStringRef attrStr = attrs.value("gender");
						wordForm.Gender = details::parseWordGender(attrStr);
					}
					if (attrs.hasAttribute("tense"))
					{
						QStringRef attrStr = attrs.value("tense");
						wordForm.Tense = details::parseWordTense(attrStr);
					}
					if (attrs.hasAttribute("degree"))
					{
						QStringRef attrStr = attrs.value("degree");
						wordForm.Degree = details::parseWordDegree(attrStr);
					}
					if (attrs.hasAttribute("mandative"))
					{
						QStringRef attrStr = attrs.value("mandative");
						wordForm.Mandative = details::parseBool(attrStr);
					}
					if (attrs.hasAttribute("number"))
					{
						QStringRef attrStr = attrs.value("number");
						wordForm.Multiplicity = details::parseWordNumber(attrStr);
					}
					if (attrs.hasAttribute("active"))
					{
						QStringRef attrStr = attrs.value("active");
						wordForm.ActiveOrPassive = details::parseWordActiveOrPassive(attrStr);
					}

					owningWordGroup->Forms.push_back(wordForm);
				}
			}
		}
	}

	bool loadUkrainianWordDeclensionXml(const std::wstring& declensionDictPath, 
		std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWords, std::wstring* errMsg)
	{
		QFile file(QString::fromStdWString(declensionDictPath));
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		{
			*errMsg = L"Can't open file for reading";
			return false;
		}

		QXmlStreamReader xml(&file);

		QTextStream dumpFileStream(&file);
		dumpFileStream.setCodec("UTF-8");

		while (!xml.atEnd())
		{
			xml.readNext();
			if (xml.isStartElement() && xml.name() == "group")
			{
				auto newGroup = std::make_unique<WordDeclensionGroup>();

				const QXmlStreamAttributes& attrs = xml.attributes();
				if (attrs.hasAttribute("name"))
				{
					QStringRef nameStr = attrs.value("name");
					QString nameQ = QString::fromRawData(nameStr.constData(), nameStr.size());
					newGroup->Name = nameQ.toStdWString();
				}
				if (attrs.hasAttribute("class"))
				{
					QStringRef attrStr = attrs.value("class");
					newGroup->WordClass = details::parseWordClass(attrStr);
				}
				if (attrs.hasAttribute("gender"))
				{
					QStringRef attrStr = attrs.value("gender");
					newGroup->Gender = details::parseWordGender(attrStr);
				}
				if (attrs.hasAttribute("number"))
				{
					QStringRef attrStr = attrs.value("number");
					newGroup->NumberCategory = details::parseWordNumber(attrStr);
				}
				if (attrs.hasAttribute("isBeing"))
				{
					QStringRef attrStr = attrs.value("isBeing");
					newGroup->IsBeing = details::parseBool(attrStr);
				}
				if (attrs.hasAttribute("person"))
				{
					QStringRef attrStr = attrs.value("person");
					newGroup->Person = details::parseWordPerson(attrStr);
				}
				if (attrs.hasAttribute("perfective"))
				{
					QStringRef attrStr = attrs.value("perfective");
					newGroup->PerfectiveAspect = details::parseWordPerfectiveAspect(attrStr);
				}
				if (attrs.hasAttribute("transitive"))
				{
					QStringRef attrStr = attrs.value("transitive");
					newGroup->Transitive = details::parseWordTransitive(attrStr);
				}
				details::parseDeclensionDictWordGroup(xml, newGroup.get());
				//declinedWords.re
				
				auto it = declinedWords.find(newGroup->Name);
				if (it == std::end(declinedWords))
				{
					// new word
					declinedWords.insert(std::make_pair(newGroup->Name, std::move(newGroup)));
				}
				else
				{
					// duplicate word
					auto& oldGroup = it->second;
					if (oldGroup->Forms.empty() && !newGroup->Forms.empty())
						declinedWords[newGroup->Name] = std::move(newGroup); // update word
					else
					{
						// ignore if old and new groups are empty
						// ignore if old is not empty and new is empty
						// ignore if old and new groups are not empty and are the same

						if (!oldGroup->Forms.empty() && !newGroup->Forms.empty() &&
							oldGroup->Forms.size() != newGroup->Forms.size())
						{
							*errMsg = L"Duplicate word groups with different word forms";
							return false;
						}
					}
				}
			}
		}
		if (xml.hasError()) {
			*errMsg = L"Error in XML stucture";
			return true;
		}
		return true;
	}

	int uniqueDeclinedWordsCount(const std::unordered_map<std::wstring, std::unique_ptr<WordDeclensionGroup>>& declinedWords)
	{
		std::unordered_set<std::wstring> uniqueWordForms;
		for (const auto& pair : declinedWords)
		{
			const WordDeclensionGroup& wordGroup = *pair.second;
			for (const WordDeclensionForm& declForm : wordGroup.Forms)
				uniqueWordForms.insert(declForm.Name);
		}
		return (int)uniqueWordForms.size();
	}
}
