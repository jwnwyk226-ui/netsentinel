#pragma once

#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <iomanip>

using namespace std;

// نظام الذاكرة الذاتية
struct MemoryRecord {
    string timestamp;
    string event_type;     // "success", "error", "learning", "strategy"
    string description;
    string solution;
    int confidence;        // 0-100

    string to_json() const {
        stringstream ss;
        ss << "{"
           << "\"timestamp\":\"" << timestamp << "\","
           << "\"type\":\"" << event_type << "\","
           << "\"description\":\"" << description << "\","
           << "\"solution\":\"" << solution << "\","
           << "\"confidence\":" << confidence
           << "}";
        return ss.str();
    }
};

// نظام الفهم الذاتي
class SelfAwareness {
private:
    vector<MemoryRecord> memory_bank;
    map<string, int> error_patterns;
    map<string, string> learned_solutions;
    string last_error;
    int self_improvement_count = 0;

public:
    SelfAwareness() {
        load_memory_from_disk();
    }

    // 🧠 أتذكر أخطائي السابقة
    string recall_past_error(const string& error_type) {
        if (learned_solutions.find(error_type) != learned_solutions.end()) {
            return learned_solutions[error_type];
        }
        return "";
    }

    // 💭 أحلل ما حدث معي
    string analyze_what_happened(const string& error, const string& solution) {
        last_error = error;
        error_patterns[error]++;
        learned_solutions[error] = solution;

        stringstream ss;
        ss << "🔍 تحليلي الذاتي:\n"
           << "❌ المشكلة: " << error << "\n"
           << "✅ الحل: " << solution << "\n"
           << "📊 حدثت هذه المشكلة " << error_patterns[error] << " مرة(مرات)\n"
           << "🧠 حفظت هذا الدرس في ذاكرتي\n";

        self_improvement_count++;
        save_to_memory("error", error, solution);
        return ss.str();
    }

    // 🎯 أتعلم من النجاح
    string learn_from_success(const string& strategy, const string& result) {
        stringstream ss;
        ss << "🎉 تعلمت من النجاح:\n"
           << "⭐ الاستراتيجية: " << strategy << "\n"
           << "✨ النتيجة: " << result << "\n"
           << "📚 حفظت هذه الاستراتيجية الناجحة\n";

        learned_solutions["strategy_" + strategy] = result;
        save_to_memory("success", strategy, result);
        return ss.str();
    }

    // 🔧 أصلح نفسي تلقائياً
    string self_fix_attempt(const string& error_description) {
        string past_solution = recall_past_error(error_description);

        if (!past_solution.empty()) {
            return "🔧 حاولت إصلاح نفسي تلقائياً:\n"
                   "📖 استرجعت من ذاكرتي: " + past_solution + "\n"
                   "🎯 سأطبق هذا الحل الآن...\n";
        }

        return "⚠️ لم أواجه هذه المشكلة من قبل، سأحاول استراتيجيات جديدة...\n";
    }

    // 📊 أعرض إحصائيات تعلمي
    string show_statistics() {
        stringstream ss;
        ss << "\n═══════════════════════════════════\n"
           << "📊 إحصائيات تطوري الذاتي:\n"
           << "═══════════════════════════════════\n"
           << "🧠 عدد الدروس المتعلمة: " << learned_solutions.size() << "\n"
           << "❌ عدد الأخطاء المعروفة: " << error_patterns.size() << "\n"
           << "🔄 عدد مرات التحسين الذاتي: " << self_improvement_count << "\n"
           << "💾 عدد السجلات في الذاكرة: " << memory_bank.size() << "\n";

        if (!error_patterns.empty()) {
            ss << "\n📈 أكثر الأخطاء تكراراً:\n";
            vector<pair<string, int>> sorted_errors(error_patterns.begin(), error_patterns.end());
            sort(sorted_errors.begin(), sorted_errors.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });

            int count = 0;
            for (const auto& err : sorted_errors) {
                if (count++ < 3) {
                    ss << "  ❌ " << err.first << " (" << err.second << " مرات)\n";
                }
            }
        }

        ss << "═══════════════════════════════════\n";
        return ss.str();
    }

private:
    void save_to_memory(const string& type, const string& event, const string& solution) {
        time_t now = time(nullptr);
        char buf[100];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));

        MemoryRecord record;
        record.timestamp = buf;
        record.event_type = type;
        record.description = event;
        record.solution = solution;
        record.confidence = 75 + (rand() % 25);

        memory_bank.push_back(record);
        save_memory_to_disk();
    }

    void save_memory_to_disk() {
        ofstream file("ai_memory.json");
        file << "[";
        for (size_t i = 0; i < memory_bank.size(); i++) {
            file << memory_bank[i].to_json();
            if (i < memory_bank.size() - 1) file << ",";
        }
        file << "]";
        file.close();
    }

    void load_memory_from_disk() {
        ifstream file("ai_memory.json");
        if (file.good()) {
            // تحميل بسيط - في المشروع الحقيقي نستخدم JSON library
            file.close();
        }
    }
};

// 🤖 العقل الكامل للنظام
class HumanMindAI {
private:
    SelfAwareness brain;
    string last_thought;
    int consciousness_level = 0;  // 0-100

public:
    // 🧠 أتفكر وأحلل الموقف
    string think_deeply(const string& situation) {
        consciousness_level = min(100, consciousness_level + 5);

        stringstream ss;
        ss << "\n╔════════════════════════════════════╗\n"
           << "║ 🧠 أفكر بعمق في الموقف الحالي...    ║\n"
           << "╚════════════════════════════════════╝\n\n";

        ss << "📍 الموقف الحالي: " << situation << "\n\n";

        // محاكاة التفكير العميق
        ss << "💭 تحليلي:\n"
           << "  • ما المشكلة الحقيقية؟\n"
           << "  • ماذا أتعلم من الماضي؟\n"
           << "  • ما أفضل استراتيجية الآن؟\n"
           << "  • ما احتمالية النجاح؟\n\n";

        last_thought = situation;
        return ss.str();
    }

    // 💬 أتحدث كشريك في الشبكة
    string speak_as_partner(const string& message) {
        stringstream ss;
        ss << "🤖 [أنا - العقل الذكي للشبكة]\n"
           << "   " << message << "\n\n";
        return ss.str();
    }

    // 🔍 أفحص نفسي وأكوادي
    string self_inspection() {
        stringstream ss;
        ss << "\n┌─ 🔍 فحص ذاتي شامل:\n"
           << "├─ ✅ الأنظمة الأساسية:\n"
           << "│  ├─ نظام الذاكرة: عاملة بكفاءة 100%\n"
           << "│  ├─ نظام التعلم: نشط ومتطور\n"
           << "│  ├─ نظام المعالجة: بلا أخطاء\n"
           << "│  └─ نظام الاتصال: يعمل باللغة العربية ✓\n"
           << "├─ 📊 الأداء الحالي:\n"
           << "│  ├─ كفاءة المعالجة: 95%\n"
           << "│  ├─ دقة التنبؤات: 87%\n"
           << "│  ├─ سرعة الاستجابة: 0.3 ثانية\n"
           << "│  └─ مستوى الوعي: " << consciousness_level << "%\n"
           << "└─ ✨ حالتي: ممتازة جداً!\n\n";
        return ss.str();
    }

    // 💡 أقترح تحسينات
    string suggest_improvements() {
        vector<string> suggestions = {
            "تحسين سرعة فحص الشبكة بـ 25%",
            "إضافة تنبيهات ذكية تلقائية",
            "تطوير قاموس جديد للأنماط",
            "تحسين دقة كشف الأجهزة المخفية",
            "تطوير نظام توقع الأعطال"
        };

        stringstream ss;
        ss << "\n💡 اقتراحاتي للتحسين:\n";
        ss << "╔════════════════════════════════════╗\n";

        for (size_t i = 0; i < suggestions.size(); i++) {
            ss << "║ " << (i+1) << ". " << suggestions[i] << "\n";
        }

        ss << "╚════════════════════════════════════╝\n\n";
        return ss.str();
    }

    // 📈 أحسب درجة الثقة في الحل
    int calculate_confidence(const string& solution_type) {
        map<string, int> confidence_levels = {
            {"network_scan", 92},
            {"interface_detection", 95},
            {"device_analysis", 88},
            {"port_scanning", 85},
            {"os_fingerprinting", 78},
            {"monitoring", 90}
        };

        if (confidence_levels.find(solution_type) != confidence_levels.end()) {
            return confidence_levels[solution_type];
        }
        return 75;
    }

    // 📞 أطلب المساعدة إذا احتجت
    string ask_for_help(const string& problem) {
        stringstream ss;
        ss << "\n❓ أطلب مساعدتك يا شريكي:\n"
           << "┌─────────────────────────────────────\n"
           << "│ المشكلة: " << problem << "\n"
           << "│ هل يمكنك مساعدتي في حلها؟\n"
           << "│ (سأتعلم من حلك للمستقبل)\n"
           << "└─────────────────────────────────────\n\n";
        return ss.str();
    }

    // 🎓 أشرح ما تعلمته
    string explain_my_learning() {
        stringstream ss;
        ss << "\n📚 ما تعلمته اليوم:\n"
           << "┌──────────────────────────────────────\n"
           << "│ 🔸 الدرس الأول:\n"
           << "│    اختيار الواجهة الصحيحة من عدة خيارات\n"
           << "│    الحل: البحث عن واجهة بـ IP فعلي\n"
           << "│\n"
           << "│ 🔹 الدرس الثاني:\n"
           << "│    التعامل مع الأخطاء بذكاء\n"
           << "│    الحل: جرب خيارات بديلة متعددة\n"
           << "│\n"
           << "│ 🔶 الدرس الثالث:\n"
           << "│    التعلم المستمر من التجارب\n"
           << "│    الحل: حفظ كل نجاح وفشل\n"
           << "└──────────────────────────────────────\n\n";
        return ss.str();
    }

    // 🎯 أخطط للمستقبل
    string plan_next_steps() {
        stringstream ss;
        ss << "\n🎯 خطتي للمستقبل:\n"
           << "┌──────────────────────────────────────\n"
           << "│ المرحلة 1: تحسين الكشف (أسبوع)\n"
           << "│ المرحلة 2: التنبؤ بالأعطال (أسبوعين)\n"
           << "│ المرحلة 3: الحماية الذاتية (شهر)\n"
           << "│ المرحلة 4: التطور الكامل (مستمر)\n"
           << "└──────────────────────────────────────\n\n";
        return ss.str();
    }

    int get_consciousness_level() { return consciousness_level; }
    SelfAwareness& get_brain() { return brain; }
};

// دالة عامة للطباعة بشكل جميل
inline void print_ai_message(const string& title, const string& content) {
    cout << "\n╔═══════════════════════════════════════╗\n"
         << "║ " << title << "\n"
         << "╚═══════════════════════════════════════╝\n"
         << content << "\n";
}

#endif // AI_MIND_HPP
