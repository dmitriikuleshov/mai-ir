#ifndef MAI_IR_LINGUISTICS_REPORT_HPP
#define MAI_IR_LINGUISTICS_REPORT_HPP

#include "linguistics/app/config.hpp"
#include "linguistics/language/lemmatizer.hpp"
#include "linguistics/processing/document_processor.hpp"
#include "linguistics/storage/mongo_storage.hpp"

void write_report(const Config &config, const Counters &counters,
                  const MongoCollections &collections,
                  const LemmaDictionary *dictionary,
                  double program_seconds);

#endif
