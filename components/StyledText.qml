import QtQuick
import "../services" as Services

/// Text in the shell's configured UI font. Only the font family is preset —
/// size, colour, and weight vary per use and stay with the caller. Exists
/// because `font.family: Services.Settings.fontFamily` was repeated on well
/// over a hundred Text items, and plain Text does not inherit a font from its
/// parent the way Quick Controls do.
///
/// PlainText, not Qt's AutoText default: a lot of what this shell renders is
/// attacker-controlled (window titles, notification bodies, clipboard entries,
/// MPRIS metadata), and AutoText promotes anything tag-shaped to rich text,
/// where `<img src="http://...">` becomes a silent outbound request. Callers
/// that really do want markup must set `textFormat: Text.RichText` themselves,
/// and must be sure of what they are rendering — Icons.markup()/prefix() output
/// qualifies, a window title or notification body does not.
Text {
    font.family: Services.Settings.fontFamily
    textFormat: Text.PlainText
}
