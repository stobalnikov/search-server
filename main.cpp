#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}


struct Document {
    int id;
    double relevance;
    int rating;
};


enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};


class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }


    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus given_status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [given_status](int document_id, DocumentStatus status, int rating) { return status == given_status; });
    }



template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query,DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }




    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };



    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;




    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }



    template <typename DocumentPredicate>


    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    map<int, double> document_to_relevance;
    for (const string& word : query.plus_words) {
    if (word_to_document_freqs_.count(word) == 0) { continue;}


            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {



                if (document_predicate (document_id, documents_.at(document_id).status, documents_.at(document_id).rating) )
                {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }



            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }
};

// ==================== для примера =========================


void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}
/* Подставьте вашу реализацию класса SearchServer сюда */




template <typename LeftValue, typename RightValue>
void AssertEqualImpl(const LeftValue& left, const RightValue& right,
    const std::string& left_expression, const std::string& right_expression,
    const std::string& file, const std::string& function_name,
    unsigned line, const std::string& hint) {
    if (left != right) {
        std::cerr << std::boolalpha;
        std::cerr << file << "(" << line << "): " << function_name << ": ";
        std::cerr << "ASSERT_EQUAL(" << left_expression << ", " << right_expression << ") failed: ";
        std::cerr << left << " != " << right << ".";

        if (!hint.empty()) {
            std::cerr << " Hint: " << hint;
        }

        std::cerr << std::endl;
        std::cerr << std::noboolalpha;

        abort();
    }
}

void AssertImpl(bool value, const std::string& expression,
    const std::string& file, const std::string& function_name,
    unsigned line, const std::string& hint) {
    if (!value) {
        std::cerr << file << "(" << line << "): " << function_name << ": ";
        std::cerr << "ASSERT(" << expression << ") failed.";
        if (!hint.empty()) {
            std::cerr << " Hint: " << hint;
        }
        std::cerr << std::endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, "")

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, "")

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

#define RUN_TEST(function) function(); std::cerr << #function " OK\n"

/*
   Подставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/




// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}




//Добавление документов. Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.
void TestAddDocument() {

        const std::vector<int> ratings = { 1 };
        const int document_id = 42;
        const std::string content1 = "cat in the city";
        const std::string content2 = "dog was found";

        
            SearchServer server;

            server.AddDocument(document_id, content1, DocumentStatus::ACTUAL, ratings);
            server.AddDocument(document_id + 1, content2, DocumentStatus::ACTUAL, ratings);

            ASSERT_EQUAL_HINT(server.GetDocumentCount(), 2, "AddDocument doesn't add documents properly");

            {
                const std::vector<Document>& result = server.FindTopDocuments(content1);

                ASSERT_EQUAL_HINT(result.size(), 1, "Document doesn't match itself");
                ASSERT_EQUAL_HINT(result[0].id, document_id, "Document content matches content of other document");
            }

            {
                const std::vector<Document>& result = server.FindTopDocuments(content2);

                ASSERT_EQUAL_HINT(result.size(), 1, "Document doesn't match itself");
                ASSERT_EQUAL_HINT(result[0].id, (document_id + 1),
                    "Document content matches content of other document");

            }

            ASSERT_HINT(server.FindTopDocuments("nothing here").empty(),
                "FindTopDocuments matches documents it must not match");
        
    
}

//Поддержка минус-слов. Документы, содержащие минус-слова из поискового запроса, не должны включаться в результаты поиска.
void TestMinusWords() {
    const int document_id = 42;
    const std::vector<int> ratings = { 1 };

    {
        SearchServer server;

        server.AddDocument(document_id, "cat in the city", DocumentStatus::ACTUAL, ratings);
        server.AddDocument(document_id + 1, "dog in the park", DocumentStatus::ACTUAL, ratings);

        {
            const auto& result = server.FindTopDocuments("in the -dog");

            ASSERT_EQUAL_HINT(result.size(), 1, "Minus words are not ignored");
            ASSERT_EQUAL_HINT(result[0].id, document_id, "Minus words are not ignored, query returned wrong document");
        }

        {
            ASSERT_HINT(server.FindTopDocuments("the -cat -dog").empty(), "Minus words are not ignored");
        }
    }
}

//Соответствие документов поисковому запросу. При этом должны быть возвращены все слова из поискового запроса, присутствующие в документе. Если есть соответствие хотя бы по одному минус-слову, должен возвращаться пустой список слов.
void TestMatchDocToRequest(){

    const int document_id = 42;
	SearchServer server;
	server.AddDocument(document_id, "cat in the city", DocumentStatus::ACTUAL, { 1 });
	{
    const auto& [matched_words, document_status] = server.MatchDocument("cat city", document_id);
    ASSERT_EQUAL_HINT(matched_words.size(), 2, "MatchDocument doesn't match all words in the document");
    ASSERT_EQUAL_HINT(matched_words[0], "cat", "MatchDocument doesn't match some words");
    ASSERT_EQUAL_HINT(matched_words[1], "city", "MatchDocument doesn't match some words");
	}
	{
    const auto& [matched_words, unused] = server.MatchDocument("cat -city", document_id);
    ASSERT_HINT(matched_words.empty(), "Minus words are not ignored in MatchDocument");
	}
}

//Сортировка найденных документов по релевантности. Возвращаемые при поиске документов результаты должны быть отсортированы в порядке убывания релевантности.
void TestSortByRelevance(){

    const int document_id = 42;
    const std::vector<int> ratings = { 1 };

    {
        SearchServer server;

        server.AddDocument(document_id, "The cat in the city", DocumentStatus::ACTUAL, ratings);
        server.AddDocument(document_id + 1, "The cat in the", DocumentStatus::ACTUAL, ratings);
        server.AddDocument(document_id + 2, "The cat in", DocumentStatus::ACTUAL, ratings);
        server.AddDocument(document_id + 3, "The", DocumentStatus::ACTUAL, ratings);

        int id = 0;
        const std::vector<std::string> words = {
            "generation", "bake", "quarrel", "ferry", "biscuit",
            "table", "bother", "guideline", "duty", "first",
        };

        for (auto word : words) {
            server.AddDocument(++id, word, DocumentStatus::ACTUAL, ratings);
        }

        {
            const auto& result = server.FindTopDocuments("cat");

            ASSERT_EQUAL_HINT(result.size(), 3, "FindTopDocument incorrect amount of documents");
            constexpr double document_relevance_inaccuracy = 1e-6;

            auto sort_by_relevance = [document_relevance_inaccuracy](const Document& left, const Document& right) {
                return std::abs(left.relevance - right.relevance) < document_relevance_inaccuracy;
            };

            ASSERT_HINT(std::is_sorted(std::begin(result), std::end(result), sort_by_relevance),
                "Documents are not sorted correctly");
        }
    }

}

//Вычисление рейтинга документов. Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void TestCalcRating(){

	auto calculate_average_rating = [](const std::vector<int>& input) {
	        int correct = 0;
	        for (auto number : input) {
	            correct += number;
	        }
	        correct /= static_cast<int>(input.size());
	        return correct;
	    };

	    {
	        const std::vector<int> ratings = { 1, 10, 28, 60, 11, 11, 12321 };

	        SearchServer server;

	        server.AddDocument(42, "cat in the city", DocumentStatus::ACTUAL, ratings);

	        {
	            const auto& result = server.FindTopDocuments("cat");

	            ASSERT_EQUAL_HINT(result[0].rating, calculate_average_rating(ratings), "Incorrect rating calculation");
	        }
	    }

	    {
	        const std::vector<int> ratings = { 545, 136, 548, 508, 797, 21005, 245 };

	        SearchServer server;

	        server.AddDocument(42, "cat in the city", DocumentStatus::ACTUAL, ratings);

	        {
	            const auto& result = server.FindTopDocuments("cat");

	            ASSERT_EQUAL_HINT(result[0].rating, calculate_average_rating(ratings), "Incorrect rating calculation");
	        }
	    }

}


//Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
void TestFiltrationByUserDefinedPredicate(){

	const int document_id = 42;
	    const std::vector<int> ratings = { 1 };

	    {
	        SearchServer server;

	        server.AddDocument(document_id, "The cat in the city", DocumentStatus::ACTUAL, ratings);
	        server.AddDocument(document_id + 1, "The cat in the", DocumentStatus::ACTUAL, ratings);
	        server.AddDocument(document_id + 2, "The cat in", DocumentStatus::ACTUAL, ratings);
	        server.AddDocument(document_id + 3, "The", DocumentStatus::ACTUAL, ratings);

	        {
	            auto get_every_second_document = [](int document_id, DocumentStatus, int) {
	                return document_id % 2 == 0;
	            };

	            const auto& result = server.FindTopDocuments("cat", get_every_second_document);

	            ASSERT_EQUAL_HINT(result.size(), 2, "Function filter is not applied correctly");

	            ASSERT_EQUAL_HINT(result[0].id, document_id + 2,
	                "Function filter is not applied correctly, wrong document");

	            ASSERT_EQUAL_HINT(result[1].id, document_id, "Fuction filter is not applied correctly, wrong document");
	        }
	    }
}

//Поиск документов, имеющих заданный статус.
void FindDocumentWithStatus(){

	const int document_id = 42;
	    const std::vector<int> ratings = { 1 };


	        SearchServer server;

	        server.AddDocument(document_id, "The cat in the city", DocumentStatus::ACTUAL, ratings);
	        server.AddDocument(document_id + 1, "The cat in the", DocumentStatus::BANNED, ratings);
	        server.AddDocument(document_id + 2, "The cat in", DocumentStatus::IRRELEVANT, ratings);
	        server.AddDocument(document_id + 3, "The cat", DocumentStatus::REMOVED, ratings);

	        {
	            const auto& result = server.FindTopDocuments("cat", DocumentStatus::REMOVED);

	            ASSERT_EQUAL_HINT(result.size(), 1, "Status filter is not applied correctly");

	            ASSERT_EQUAL_HINT(result[0].id, (document_id + 3),
	                "Status filter is not applied correctly, wrong document");
	        }

}

//Корректное вычисление релевантности найденных документов.
void TestCalcRelevance(){

	const int document_id = 42;
	    const std::vector<int> ratings = { 1 };


	        SearchServer server;

	        server.AddDocument(document_id, "The cat in the city", DocumentStatus::ACTUAL, ratings);
	        server.AddDocument(document_id + 1, "The cat in the", DocumentStatus::ACTUAL, ratings);
	        server.AddDocument(document_id + 2, "The cat in", DocumentStatus::ACTUAL, ratings);
	        server.AddDocument(document_id + 3, "The", DocumentStatus::ACTUAL, ratings);

	        {
	            const auto& result = server.FindTopDocuments("cat");

	            ASSERT_EQUAL_HINT(result.size(), 3, "Incorrect amount of returned documents");

	            const double idf = log(4.0 / 3.0);

	            ASSERT_EQUAL_HINT(result[0].relevance, (1.0 / 3.0) * idf, "Relevance is not calculated correctly");

	            ASSERT_EQUAL_HINT(result[1].relevance, (1.0 / 4.0) * idf, "Relevance is not calculated correctly");

	            ASSERT_EQUAL_HINT(result[2].relevance, (1.0 / 5.0) * idf, "Relevance is not calculated correctly");
	        }

}
/*
Разместите код остальных тестов здесь
*/

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestMatchDocToRequest);
    RUN_TEST(TestSortByRelevance);
    RUN_TEST(TestCalcRating);
    RUN_TEST(TestFiltrationByUserDefinedPredicate);
    RUN_TEST(FindDocumentWithStatus);
    RUN_TEST(TestCalcRelevance);

    // Не забудьте вызывать остальные тесты здесь
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
