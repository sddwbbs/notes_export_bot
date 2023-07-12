#include <iostream>
#include <tgbot/tgbot.h>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <cpr/cpr.h>
#include <algorithm>
#include <nlohmann/json.hpp>

using std::string;
using std::vector;
using namespace TgBot;
using json = nlohmann::json;

vector<string>bot_commands = {"export", "start"};
string notes_text;
string notes_text_without_name;
bool is_notes_export = false;

std::string encodeBase64(const std::string& input) {
    std::vector<unsigned char> bytes(input.begin(), input.end());

    std::string encoded;
    auto base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int bits = 0;
    int value = 0;
    for (unsigned char c : bytes) {
        value = (value << 8) + c;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            encoded.push_back(base64_chars[(value >> bits) & 0x3F]);
        }
    }
    if (bits > 0) {
        value <<= 6 - bits;
        encoded.push_back(base64_chars[value & 0x3F]);
    }
    while (encoded.size() % 4 != 0) {
        encoded.push_back('=');
    }

    return encoded;
}

const string githubUsername = "sddwbbs";
const string githubToken = getenv("GITHUB_TOKEN");

bool checkFileExistence(const string& book_name);

bool createNewNote(const string& book_name) {
    std::string encoded_name = cpr::util::urlEncode(book_name);
    std::string encoded_text = encodeBase64("### " + book_name + "  \n" + notes_text_without_name);
    std::string url = "https://api.github.com/repos/" + githubUsername + "/Obsidian_Notes/contents/books/" + encoded_name + ".md";

    json request_body;
    request_body["message"] = "Create new note";
    request_body["committer"] = {
            {"name", "Ivan Gabrusevich"},
            {"email", "ivan@gabrusevich.ru"}
    };
    request_body["content"] = encoded_text;

    std::string request_body_string = request_body.dump();

    cpr::Response r = cpr::Put(cpr::Url{url},
                               cpr::Header{{"Accept", "application/vnd.github+json"},
                                           {"Authorization", "Bearer " + githubToken},
                                           {"X-GitHub-Api-Version", "2022-11-28"}},
                               cpr::Body{request_body_string},
                               cpr::VerifySsl{false});

    if (checkFileExistence(book_name)) {
        return true;
    } else {
        return false;
    }
}

bool editNote(const string& book_name) {
    std::string encoded_name = cpr::util::urlEncode(book_name);
    std::string url = "https://api.github.com/repos/" + githubUsername + "/Obsidian_Notes/contents/books/" + encoded_name + ".md?ref=main";
    std::string encoded_text = encodeBase64("### " + book_name + "  \n" + notes_text_without_name);

    cpr::Response r1 = cpr::Get(cpr::Url{url},
                                cpr::Header{{"Accept", "application/vnd.github+json"},
                                            {"Authorization", "Bearer " + githubToken},
                                            {"X-GitHub-Api-Version", "2022-11-28"}},
                                cpr::VerifySsl{false});

    if (r1.status_code == 404) {
        return false;
    }

    json response_json = json::parse(r1.text);
    std::string sha_value = response_json["sha"];

    json request_body;
    request_body["message"] = "Edit note";
    request_body["committer"] = {
            {"name", "Ivan Gabrusevich"},
            {"email", "ivan@gabrusevich.ru"}
    };
    request_body["content"] = encoded_text;
    request_body["sha"] = sha_value;

    std::string request_body_string = request_body.dump();

    cpr::Response r2 = cpr::Put(cpr::Url{url},
                                cpr::Header{{"Accept", "application/vnd.github+json"},
                                            {"Authorization", "Bearer " + githubToken},
                                            {"X-GitHub-Api-Version", "2022-11-28"}},
                                cpr::Body{request_body_string},
                                cpr::VerifySsl{false});

    return true;
}

bool checkFileExistence(const string& book_name) {
    std::string encoded_name = cpr::util::urlEncode(book_name);
    std::string url = "https://api.github.com/repos/" + githubUsername + "/Obsidian_Notes/contents/books/" + encoded_name + ".md?ref=main";

    cpr::Response r = cpr::Get(cpr::Url{url},
                               cpr::Header{{"Accept", "application/vnd.github+json"},
                                           {"Authorization", "Bearer " + githubToken},
                                           {"X-GitHub-Api-Version", "2022-11-28"}},
                               cpr::VerifySsl{false});

    if (r.status_code == 200) {
        std::cout << "File exists" << std::endl;
        return true;
    } else if (r.status_code == 404) {
        std::cout << "File does not exist" << std::endl;
        return false;
    } else {
        std::cout << "Error: " << r.status_code << std::endl;
        return false;
    }
}

void saveUserText(Bot &bot, Message::Ptr message) {
    notes_text = message->text;
    bot.getApi().sendMessage(message->chat->id, "Notes saved successfully!");
    is_notes_export = false;

    string book_name;
    size_t space_pos = notes_text.find_first_of(' ');
    string first_word = notes_text.substr(0, space_pos);
    notes_text_without_name = notes_text.substr(space_pos, notes_text.length());
    std::replace(first_word.begin(), first_word.end(), '_', ' ');
    book_name = first_word;

    if (checkFileExistence(book_name)) {
        bot.getApi().sendMessage(message->chat->id, "Book already exist");
        if (editNote(book_name)) {
            bot.getApi().sendMessage(message->chat->id, "Note edited");
        } else {
            bot.getApi().sendMessage(message->chat->id, "Error");
        }
    } else {
        bot.getApi().sendMessage(message->chat->id, "Book does not exist");
        if (createNewNote(book_name)) {
            bot.getApi().sendMessage(message->chat->id, "Note added");
        } else {
            bot.getApi().sendMessage(message->chat->id, "Something went wrong");
        }
    }
}

int main() {
    string token(getenv("TOKEN"));
    Bot bot(token);

    InlineKeyboardMarkup::Ptr keyboard(new InlineKeyboardMarkup);
    vector<InlineKeyboardButton::Ptr> buttons;
    InlineKeyboardButton::Ptr export_btn(new InlineKeyboardButton);
    export_btn->text = "Export notes";
    export_btn->callbackData = "exp_notes";
    buttons.push_back(export_btn);
    keyboard->inlineKeyboard.push_back(buttons);

    bot.getEvents().onCommand("start", [&bot, &keyboard](Message::Ptr message) {
        bot.getApi().sendMessage(message->chat->id, "Hi!", false, 0, keyboard);
    });

    bot.getEvents().onCommand("export", [&bot](Message::Ptr message) {
       bot.getApi().sendMessage(message->chat->id, "Send your text");
       is_notes_export = true;
    });

    bot.getEvents().onCallbackQuery([&bot](CallbackQuery::Ptr query) {
        if (query->data == "exp_notes") {
            bot.getApi().sendMessage(query->message->chat->id, "Send your text");
            is_notes_export = true;
        }
    });

    bot.getEvents().onAnyMessage([&bot](Message::Ptr message) {
        printf("User wrote %s\n", message->text.c_str());

        if (is_notes_export) {
            saveUserText(bot, message);
            return;
        }

        for (const auto& command : bot_commands) {
            if ("/" + command == message->text) {
                return;
            }
        }

        bot.getApi().sendMessage(message->chat->id, "Sorry, there is no such command... " + message->text);
    });

    try {
        printf("Bot username: %s\n", bot.getApi().getMe()->username.c_str());
        TgLongPoll longPoll(bot);
        while (true) {
            printf("Long poll started\n");
            longPoll.start();
        }
    } catch (TgException& e) {
        printf("error: %s\n", e.what());
    }

    return 0;
}
