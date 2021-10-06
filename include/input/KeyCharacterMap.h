/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _LIBINPUT_KEY_CHARACTER_MAP_H
#define _LIBINPUT_KEY_CHARACTER_MAP_H

#include <stdint.h>

#ifdef __linux__
#include <binder/IBinder.h>
#endif

#include <android-base/result.h>
#include <input/Input.h>
#include <utils/Errors.h>
#include <utils/KeyedVector.h>
#include <utils/Tokenizer.h>
#include <utils/Unicode.h>

// Maximum number of keys supported by KeyCharacterMaps
#define MAX_KEYS 8192

namespace android {

/**
 * Describes a mapping from Android key codes to characters.
 * Also specifies other functions of the keyboard such as the keyboard type
 * and key modifier semantics.
 *
 * This object is immutable after it has been loaded.
 */
class KeyCharacterMap {
public:
    enum class KeyboardType : int32_t {
        UNKNOWN = 0,
        NUMERIC = 1,
        PREDICTIVE = 2,
        ALPHA = 3,
        FULL = 4,
        /**
         * Deprecated. Set 'keyboard.specialFunction' to '1' in the device's IDC file instead.
         */
        SPECIAL_FUNCTION = 5,
        OVERLAY = 6,
    };

    enum class Format {
        // Base keyboard layout, may contain device-specific options, such as "type" declaration.
        BASE = 0,
        // Overlay keyboard layout, more restrictive, may be published by applications,
        // cannot override device-specific options.
        OVERLAY = 1,
        // Either base or overlay layout ok.
        ANY = 2,
    };

    // Substitute key code and meta state for fallback action.
    struct FallbackAction {
        int32_t keyCode;
        int32_t metaState;
    };

    /* Loads a key character map from a file. */
    static base::Result<std::shared_ptr<KeyCharacterMap>> load(const std::string& filename,
                                                               Format format);

    /* Loads a key character map from its string contents. */
    static base::Result<std::shared_ptr<KeyCharacterMap>> loadContents(const std::string& filename,
                                                                       const char* contents,
                                                                       Format format);

    const std::string getLoadFileName() const;

    /* Combines this key character map with an overlay. */
    void combine(const KeyCharacterMap& overlay);

    /* Gets the keyboard type. */
    KeyboardType getKeyboardType() const;

    /* Gets the primary character for this key as in the label physically printed on it.
     * Returns 0 if none (eg. for non-printing keys). */
    char16_t getDisplayLabel(int32_t keyCode) const;

    /* Gets the Unicode character for the number or symbol generated by the key
     * when the keyboard is used as a dialing pad.
     * Returns 0 if no number or symbol is generated.
     */
    char16_t getNumber(int32_t keyCode) const;

    /* Gets the Unicode character generated by the key and meta key modifiers.
     * Returns 0 if no character is generated.
     */
    char16_t getCharacter(int32_t keyCode, int32_t metaState) const;

    /* Gets the fallback action to use by default if the application does not
     * handle the specified key.
     * Returns true if an action was available, false if none.
     */
    bool getFallbackAction(int32_t keyCode, int32_t metaState,
            FallbackAction* outFallbackAction) const;

    /* Gets the first matching Unicode character that can be generated by the key,
     * preferring the one with the specified meta key modifiers.
     * Returns 0 if no matching character is generated.
     */
    char16_t getMatch(int32_t keyCode, const char16_t* chars,
            size_t numChars, int32_t metaState) const;

    /* Gets a sequence of key events that could plausibly generate the specified
     * character sequence.  Returns false if some of the characters cannot be generated.
     */
    bool getEvents(int32_t deviceId, const char16_t* chars, size_t numChars,
            Vector<KeyEvent>& outEvents) const;

    /* Maps a scan code and usage code to a key code, in case this key map overrides
     * the mapping in some way. */
    status_t mapKey(int32_t scanCode, int32_t usageCode, int32_t* outKeyCode) const;

    /* Tries to find a replacement key code for a given key code and meta state
     * in character map. */
    void tryRemapKey(int32_t scanCode, int32_t metaState,
            int32_t* outKeyCode, int32_t* outMetaState) const;

#ifdef __linux__
    /* Reads a key map from a parcel. */
    static std::shared_ptr<KeyCharacterMap> readFromParcel(Parcel* parcel);

    /* Writes a key map to a parcel. */
    void writeToParcel(Parcel* parcel) const;
#endif

    bool operator==(const KeyCharacterMap& other) const;

    KeyCharacterMap(const KeyCharacterMap& other);

    virtual ~KeyCharacterMap();

private:
    struct Behavior {
        Behavior();
        Behavior(const Behavior& other);

        /* The next behavior in the list, or NULL if none. */
        Behavior* next;

        /* The meta key modifiers for this behavior. */
        int32_t metaState;

        /* The character to insert. */
        char16_t character;

        /* The fallback keycode if the key is not handled. */
        int32_t fallbackKeyCode;

        /* The replacement keycode if the key has to be replaced outright. */
        int32_t replacementKeyCode;
    };

    struct Key {
        Key();
        Key(const Key& other);
        ~Key();

        /* The single character label printed on the key, or 0 if none. */
        char16_t label;

        /* The number or symbol character generated by the key, or 0 if none. */
        char16_t number;

        /* The list of key behaviors sorted from most specific to least specific
         * meta key binding. */
        Behavior* firstBehavior;
    };

    class Parser {
        enum State {
            STATE_TOP = 0,
            STATE_KEY = 1,
        };

        enum {
            PROPERTY_LABEL = 1,
            PROPERTY_NUMBER = 2,
            PROPERTY_META = 3,
        };

        struct Property {
            inline explicit Property(int32_t property = 0, int32_t metaState = 0) :
                    property(property), metaState(metaState) { }

            int32_t property;
            int32_t metaState;
        };

        KeyCharacterMap* mMap;
        Tokenizer* mTokenizer;
        Format mFormat;
        State mState;
        int32_t mKeyCode;

    public:
        Parser(KeyCharacterMap* map, Tokenizer* tokenizer, Format format);
        ~Parser();
        status_t parse();

    private:
        status_t parseType();
        status_t parseMap();
        status_t parseMapKey();
        status_t parseKey();
        status_t parseKeyProperty();
        status_t finishKey(Key* key);
        status_t parseModifier(const std::string& token, int32_t* outMetaState);
        status_t parseCharacterLiteral(char16_t* outCharacter);
    };

    KeyedVector<int32_t, Key*> mKeys;
    KeyboardType mType;
    std::string mLoadFileName;

    KeyedVector<int32_t, int32_t> mKeysByScanCode;
    KeyedVector<int32_t, int32_t> mKeysByUsageCode;

    KeyCharacterMap();

    bool getKey(int32_t keyCode, const Key** outKey) const;
    bool getKeyBehavior(int32_t keyCode, int32_t metaState,
            const Key** outKey, const Behavior** outBehavior) const;
    static bool matchesMetaState(int32_t eventMetaState, int32_t behaviorMetaState);

    bool findKey(char16_t ch, int32_t* outKeyCode, int32_t* outMetaState) const;

    static base::Result<std::shared_ptr<KeyCharacterMap>> load(Tokenizer* tokenizer, Format format);

    static void addKey(Vector<KeyEvent>& outEvents,
            int32_t deviceId, int32_t keyCode, int32_t metaState, bool down, nsecs_t time);
    static void addMetaKeys(Vector<KeyEvent>& outEvents,
            int32_t deviceId, int32_t metaState, bool down, nsecs_t time,
            int32_t* currentMetaState);
    static bool addSingleEphemeralMetaKey(Vector<KeyEvent>& outEvents,
            int32_t deviceId, int32_t metaState, bool down, nsecs_t time,
            int32_t keyCode, int32_t keyMetaState,
            int32_t* currentMetaState);
    static void addDoubleEphemeralMetaKey(Vector<KeyEvent>& outEvents,
            int32_t deviceId, int32_t metaState, bool down, nsecs_t time,
            int32_t leftKeyCode, int32_t leftKeyMetaState,
            int32_t rightKeyCode, int32_t rightKeyMetaState,
            int32_t eitherKeyMetaState,
            int32_t* currentMetaState);
    static void addLockedMetaKey(Vector<KeyEvent>& outEvents,
            int32_t deviceId, int32_t metaState, nsecs_t time,
            int32_t keyCode, int32_t keyMetaState,
            int32_t* currentMetaState);
};

} // namespace android

#endif // _LIBINPUT_KEY_CHARACTER_MAP_H
