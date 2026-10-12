#include "PluginEditor.h"

using namespace rhui;

namespace
{
juce::Font font (float h, bool bold = false)
{
    auto o = juce::FontOptions (h);
    return juce::Font (bold ? o.withStyle ("Bold") : o);
}

juce::Colour sectionColour (rh::SecType t)
{
    static const juce::uint32 c[rh::NumSecTypes] = { 0xff5b6b7a, 0xff3f7a6a, 0xffb7832f, 0xffc8452d, 0xff7a5aa0, 0xff55606a };
    return juce::Colour (c[t]);
}
} // namespace

// ====================================================================== Look
Look::Look()
{
    setColour (juce::ComboBox::backgroundColourId, col::panel2);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, col::edge);
    setColour (juce::ComboBox::arrowColourId, col::amber);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::amber.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, col::bg);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::TextButton::buttonColourId, col::panel2);
    setColour (juce::TextButton::buttonOnColourId, col::red);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, col::text);
    setColour (juce::ToggleButton::textColourId, col::text);
    setColour (juce::ToggleButton::tickColourId, col::amber);
    setColour (juce::ToggleButton::tickDisabledColourId, col::dim);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::BubbleComponent::backgroundColourId, col::panel2);
    setColour (juce::BubbleComponent::outlineColourId, col::amber);
    setColour (juce::TooltipWindow::backgroundColourId, col::panel2);
    setColour (juce::TooltipWindow::textColourId, col::text);
    setColour (juce::TooltipWindow::outlineColourId, col::edge);
    setColour (juce::AlertWindow::backgroundColourId, col::panel);
    setColour (juce::AlertWindow::textColourId, col::text);
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = a0 + pos * (a1 - a0);

    juce::Path track, val;
    track.addCentredArc (c.x, c.y, r - 2.f, r - 2.f, 0.f, a0, a1, true);
    val.addCentredArc (c.x, c.y, r - 2.f, r - 2.f, 0.f, a0, ang, true);
    g.setColour (col::edge);
    g.strokePath (track, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col::amber);
    g.strokePath (val, juce::PathStrokeType (3.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float kr = r - 7.f;
    juce::ColourGradient grad (col::panel2.brighter (0.25f), c.x, c.y - kr, col::bg, c.x, c.y + kr, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f);
    g.setColour (col::edge);
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.f, kr * 2.f, 1.f);

    juce::Path p;
    p.addRoundedRectangle (-1.5f, -kr + 3.f, 3.f, kr * 0.55f, 1.5f);
    g.setColour (col::text);
    g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto base = b.getToggleState() ? b.findColour (juce::TextButton::buttonOnColourId) : b.findColour (juce::TextButton::buttonColourId);
    if (down) base = base.darker (0.2f);
    else if (over) base = base.brighter (0.12f);
    g.setColour (base);
    g.fillRoundedRectangle (r, 5.f);
    g.setColour (b.getToggleState() ? base.brighter (0.4f) : col::edge.brighter (over ? 0.4f : 0.f));
    g.drawRoundedRectangle (r, 5.f, 1.f);
}

juce::Font Look::getTextButtonFont (juce::TextButton&, int h) { return font (std::min (14.f, (float) h * 0.45f), true); }
juce::Font Look::getComboBoxFont (juce::ComboBox&) { return font (14.f); }
juce::Font Look::getPopupMenuFont() { return font (14.f); }

void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (col::panel2);
    g.fillRoundedRectangle (r, 5.f);
    g.setColour (box.hasKeyboardFocus (true) ? col::amber : col::edge);
    g.drawRoundedRectangle (r, 5.f, 1.f);
    juce::Path a;
    const float ax = (float) w - 16.f, ay = (float) h * 0.5f;
    a.addTriangle (ax - 4.f, ay - 2.f, ax + 4.f, ay - 2.f, ax, ay + 3.f);
    g.setColour (col::amber);
    g.fillPath (a);
}

// ====================================================================== Timeline
void Timeline::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (col::panel);
    g.fillRoundedRectangle (area, 8.f);
    g.setColour (col::edge);
    g.drawRoundedRectangle (area.reduced (0.5f), 8.f, 1.f);
    if (! song || song->totalBars <= 0) return;

    auto inner = area.reduced (10.f, 10.f);
    const auto ruler = inner.removeFromBottom (14.f);
    const float ppb = inner.getWidth() / (float) song->totalBars;

    for (size_t i = 0; i < song->sections.size(); ++i)
    {
        const auto& s = song->sections[i];
        auto r = juce::Rectangle<float> (inner.getX() + (float) s.startBar * ppb, inner.getY(), (float) s.bars * ppb, inner.getHeight()).reduced (1.5f, 0);
        const auto c = sectionColour (s.type);
        g.setColour (c.withAlpha (0.22f));
        g.fillRoundedRectangle (r, 5.f);
        g.setColour (c);
        g.fillRoundedRectangle (r.withHeight (4.f), 2.f);
        if ((int) i == selected)
        {
            g.setColour (col::amber);
            g.drawRoundedRectangle (r, 5.f, 2.f);
        }

        auto t = r.reduced (6.f, 8.f);
        g.setColour (col::text);
        g.setFont (font (r.getWidth() < 70 ? 10.5f : 12.5f, true));
        g.drawText (rh::sectionName (s.type), t.removeFromTop (16.f), juce::Justification::topLeft, true);
        g.setColour (col::dim);
        g.setFont (font (10.5f));
        g.drawText (juce::String (s.bars) + " bars", t.removeFromTop (14.f), juce::Justification::topLeft, true);
        t.removeFromTop (4.f);

        for (auto& ch : s.chords)
        {
            if (t.getHeight() < 14.f) break;
            auto line = t.removeFromTop (17.f);
            g.setColour (col::text);
            g.setFont (font (13.f, true));
            g.drawText (ch.name, line, juce::Justification::centredLeft, true);
            if (r.getWidth() >= 92.f)
            {
                g.setColour (c.brighter (0.6f));
                g.setFont (font (11.f));
                g.drawText (ch.roman, line, juce::Justification::centredRight, true);
            }
        }
    }

    // bar ruler
    for (int b = 0; b <= song->totalBars; ++b)
    {
        const float x = ruler.getX() + (float) b * ppb;
        const bool major = b % 4 == 0;
        g.setColour (major ? col::dim : col::edge);
        g.drawVerticalLine ((int) x, ruler.getY() + (major ? 2.f : 7.f), ruler.getBottom());
        if (major && b < song->totalBars && ppb * 4 > 22)
        {
            g.setFont (font (9.5f));
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (x + 2.f, ruler.getY(), 30.f, ruler.getHeight()), juce::Justification::centredLeft, false);
        }
    }

    if (isPlaying)
    {
        const float x = inner.getX() + (float) (head / 4.0) * ppb;
        g.setColour (col::red);
        g.fillRect (x - 1.f, inner.getY() - 4.f, 2.f, inner.getHeight() + ruler.getHeight() + 4.f);
    }
}

void Timeline::mouseDown (const juce::MouseEvent& e)
{
    if (! song || song->totalBars <= 0) return;
    auto inner = getLocalBounds().toFloat().reduced (10.f, 10.f);
    const float ppb = inner.getWidth() / (float) song->totalBars;
    const int bar = (int) ((e.position.x - inner.getX()) / ppb);
    for (size_t i = 0; i < song->sections.size(); ++i)
    {
        const auto& s = song->sections[i];
        if (bar >= s.startBar && bar < s.startBar + s.bars)
        {
            selected = (int) i;
            repaint();
            if (onSelect) onSelect (selected);
            return;
        }
    }
}

// ====================================================================== DragTile
void DragTile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.f);
    const bool hot = isMouseOver() || dragging;
    g.setColour (hot ? col::panel2.brighter (0.08f) : col::panel2);
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (accent.withAlpha (hot ? 1.f : 0.6f));
    juce::Path dash;
    dash.addRoundedRectangle (r, 6.f);
    const float pattern[] = { 5.f, 3.f };
    juce::PathStrokeType (1.4f).createDashedStroke (dash, dash, pattern, 2);
    g.fillPath (dash);

    auto icon = r.removeFromLeft (40.f);
    g.setColour (accent);
    const auto ic = icon.getCentre();
    juce::Path a;
    a.startNewSubPath (ic.x, ic.y - 9.f); a.lineTo (ic.x, ic.y + 7.f);
    a.startNewSubPath (ic.x - 6.f, ic.y + 1.f); a.lineTo (ic.x, ic.y + 7.f); a.lineTo (ic.x + 6.f, ic.y + 1.f);
    g.strokePath (a, juce::PathStrokeType (2.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (col::text);
    g.setFont (font (13.f, true));
    g.drawText (title, r.removeFromTop (r.getHeight() * 0.55f), juce::Justification::bottomLeft, true);
    g.setColour (col::dim);
    g.setFont (font (11.f));
    g.drawText ("drag onto a track", r, juce::Justification::topLeft, true);
}

void DragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 5 || ! makeFile) return;
    dragging = true;
    repaint();
    const auto f = makeFile();
    if (f.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
                                                                    [safe = juce::Component::SafePointer<DragTile> (this)]
                                                                    { if (safe) { safe->dragging = false; safe->repaint(); } });
}

// ====================================================================== Editor
RiffhouseEditor::RiffhouseEditor (RiffhouseProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    tips = std::make_unique<juce::TooltipWindow> (this, 500);

    addCombo (style, styleL, "STYLE", "style", rh::styleNames(), styleA);
    addCombo (key, keyL, "KEY", "key", rh::keyNames(), keyA);
    addCombo (mode, modeL, "MODE", "mode", rh::modeNames(), modeA);
    addCombo (structure, structureL, "STRUCTURE", "structure", rh::structureNames(), structureA);
    addCombo (voicing, voicingL, "VOICING", "voicing", rh::voicingNames(), voicingA);
    addCombo (midiIn, midiInL, "MIDI IN PLAYS", "midiIn", { "By Channel", "Guitar", "Bass", "Drums" }, midiInA);

    addKnob (humanize, "HUMANIZE", "humanize");
    addKnob (gDrive, "DRIVE", "gDrive"); addKnob (gTone, "TONE", "gTone"); addKnob (gWidth, "WIDTH", "gWidth"); addKnob (gVol, "VOLUME", "gVol");
    addKnob (bDrive, "DRIVE", "bDrive"); addKnob (bTone, "TONE", "bTone"); addKnob (bVol, "VOLUME", "bVol");
    addKnob (dPunch, "PUNCH", "dPunch"); addKnob (dRoom, "ROOM", "dRoom"); addKnob (dVol, "VOLUME", "dVol");
    addKnob (master, "MASTER", "master");

    generateBtn.setColour (juce::TextButton::buttonColourId, col::red);
    generateBtn.setTooltip ("Write a brand-new song: new chords, riffs, bass lines and beats for every section");
    generateBtn.onClick = [this] { proc.generateNew(); };
    rerollBtn.setTooltip ("New chords and patterns for the selected section type only (click a section above to select it)");
    rerollBtn.onClick = [this]
    {
        if (auto s = proc.getSong())
            if (timeline.selected >= 0 && timeline.selected < (int) s->sections.size())
                proc.rerollSection (s->sections[(size_t) timeline.selected].type);
    };
    previewBtn.setClickingTogglesState (true);
    previewBtn.setTooltip ("Play the song without pressing play in Ableton (uses the host tempo)");
    previewBtn.onClick = [this]
    {
        proc.preview = previewBtn.getToggleState();
        previewBtn.setButtonText (previewBtn.getToggleState() ? "STOP" : "PREVIEW");
    };
    saveBtn.setTooltip ("Save guitar, bass, drums and full-song MIDI files to a folder");
    saveBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose a folder for the MIDI files",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory));
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto dir = fc.getResult();
                                  if (dir == juce::File()) return;
                                  int ok = 0;
                                  for (int m : { 1, 2, 4, 7 }) ok += proc.writeMidiFile (m, dir).existsAsFile() ? 1 : 0;
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Riffhouse",
                                                                          "Saved " + juce::String (ok) + " MIDI files to\n" + dir.getFullPathName());
                              });
    };
    autoPlayA = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "autoplay", autoPlay);
    autoPlay.setTooltip ("On: the song plays in sync with Ableton's transport. Off: Riffhouse only plays MIDI you send it.");

    for (auto* c : std::initializer_list<juce::Component*> { &generateBtn, &rerollBtn, &previewBtn, &saveBtn, &autoPlay, &timeline, &info, &tempoHint,
                                                             &gTile, &bTile, &dTile, &songTile })
        addAndMakeVisible (c);

    info.setFont (font (13.5f));
    info.setJustificationType (juce::Justification::topLeft);
    info.setColour (juce::Label::textColourId, col::text);
    tempoHint.setFont (font (12.5f));
    tempoHint.setJustificationType (juce::Justification::centredRight);
    tempoHint.setColour (juce::Label::textColourId, col::dim);

    timeline.onSelect = [this] (int) { refreshSong(); };

    gTile.makeFile = [this] { return proc.writeMidiFile (1, midiFolder()); };
    bTile.makeFile = [this] { return proc.writeMidiFile (2, midiFolder()); };
    dTile.makeFile = [this] { return proc.writeMidiFile (4, midiFolder()); };
    songTile.makeFile = [this] { return proc.writeMidiFile (7, midiFolder()); };
    gTile.setTooltip ("Drag the guitar part onto a MIDI track");
    bTile.setTooltip ("Drag the bass part onto a MIDI track");
    dTile.setTooltip ("Drag the drum part onto a Drum Rack track (General MIDI notes: C1 kick, D1 snare, F#1 hat)");
    songTile.setTooltip ("Drag all three parts at once (multi-track MIDI file)");

    proc.addChangeListener (this);
    setSize (1000, 690);
    refreshSong();
    timerCallback();
    startTimerHz (30);
}

RiffhouseEditor::~RiffhouseEditor()
{
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void RiffhouseEditor::addCombo (juce::ComboBox& box, juce::Label& lab, const juce::String& caption, const juce::String& id,
                                const juce::StringArray& items, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att)
{
    box.addItemList (items, 1);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, box);
    lab.setText (caption, juce::dontSendNotification);
    lab.setFont (font (11.f, true));
    lab.setColour (juce::Label::textColourId, col::dim);
    addAndMakeVisible (box);
    addAndMakeVisible (lab);
}

void RiffhouseEditor::addKnob (Knob& k, const juce::String& caption, const juce::String& id)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.slider.setPopupDisplayEnabled (true, true, this);
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    k.label.setText (caption, juce::dontSendNotification);
    k.label.setFont (font (11.f, true));
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setColour (juce::Label::textColourId, col::dim);
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
}

juce::File RiffhouseEditor::midiFolder() const
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Riffhouse MIDI");
}

void RiffhouseEditor::changeListenerCallback (juce::ChangeBroadcaster*) { refreshSong(); }

void RiffhouseEditor::refreshSong()
{
    auto s = proc.getSong();
    timeline.setSong (s);
    if (! s || s->sections.empty()) { info.setText ({}, juce::dontSendNotification); return; }
    timeline.selected = juce::jlimit (0, (int) s->sections.size() - 1, timeline.selected);
    const auto& sec = s->sections[(size_t) timeline.selected];

    juce::StringArray names, romans;
    for (auto& c : sec.chords) { names.add (c.name); romans.add (c.roman); }
    juce::String t;
    t << rh::sectionName (sec.type) << "   " << sec.bars << " bars   " << names.joinIntoString ("  -  ")
      << "   (" << romans.joinIntoString (" - ") << ")\n";
    t << "Guitar: " << rh::guitarPatName (sec.gPat) << "     Bass: " << rh::bassPatName (sec.bPat)
      << "     Drums: " << rh::drumPatName (sec.dPat) << "\n";
    t << "Tip: " << rh::sectionTip (*s, timeline.selected);
    info.setText (t, juce::dontSendNotification);
    timeline.repaint();
}

void RiffhouseEditor::timerCallback()
{
    timeline.setPlayhead (proc.uiBeat.load(), proc.uiPlaying.load());
    if (auto s = proc.getSong())
    {
        const auto txt = "Style tempo " + juce::String (s->suggestedBpm) + " BPM     Host " + juce::String (proc.lastBpm.load(), 1) + " BPM";
        if (tempoHint.getText() != txt) tempoHint.setText (txt, juce::dontSendNotification);
    }
}

void RiffhouseEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);

    // header
    auto head = getLocalBounds().removeFromTop (58).reduced (18, 0);
    g.setColour (col::amber);
    g.setFont (font (28.f, true));
    g.drawText ("RIFFHOUSE", head.removeFromLeft (190), juce::Justification::centredLeft, false);
    g.setColour (col::dim);
    g.setFont (font (13.f));
    g.drawText ("STUDIO  /  rock songwriting instrument", head.removeFromLeft (320), juce::Justification::centredLeft, false);
    g.setColour (col::edge);
    g.drawHorizontalLine (58, 0.f, (float) getWidth());

    auto drawPanel = [&] (juce::Rectangle<int> r, const juce::String& title, const juce::String& sub)
    {
        g.setColour (col::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.f);
        g.setColour (col::edge);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.f, 1.f);
        auto t = r.reduced (14, 10).removeFromTop (20);
        g.setColour (col::amber);
        g.setFont (font (14.f, true));
        g.drawText (title, t, juce::Justification::centredLeft, false);
        g.setColour (col::dim);
        g.setFont (font (11.f));
        g.drawText (sub, t, juce::Justification::centredRight, false);
    };
    drawPanel (gPanel, "GUITAR", "modelled strings > amp > cab");
    drawPanel (bPanel, "BASS", "string + sub > comp");
    drawPanel (dPanel, "DRUMS", "synth rock kit");
    drawPanel (sPanel, "SONG", "export + routing");

    auto hint = [&] (juce::Rectangle<int> panel, const juce::String& t)
    {
        g.setColour (col::dim);
        g.setFont (font (11.5f));
        g.drawFittedText (t, panel.reduced (16, 0).withTrimmedTop (140).withHeight (52), juce::Justification::topLeft, 3);
    };
    hint (gPanel, "Low drive + high tone = clean indie jangle. Drive past 70% for metal. Width spreads the double-tracked guitars left and right.");
    hint (bPanel, "Drive adds grit that cuts through. Lower the tone for a round, sub-heavy bass.");
    hint (dPanel, "Punch sharpens the kick and squashes the kit. Room adds a live-room sound.");
}

void RiffhouseEditor::resized()
{
    // header controls
    auto head = getLocalBounds().removeFromTop (58).reduced (18, 13);
    previewBtn.setBounds (head.removeFromRight (110));
    head.removeFromRight (12);
    tempoHint.setBounds (head.removeFromRight (280));

    // generator row
    auto row = juce::Rectangle<int> (18, 68, getWidth() - 36, 50);
    auto place = [&] (juce::ComboBox& b, juce::Label& l, int w)
    {
        auto r = row.removeFromLeft (w);
        l.setBounds (r.removeFromTop (18).withTrimmedLeft (-2));
        b.setBounds (r.withHeight (30));
        row.removeFromLeft (10);
    };
    place (style, styleL, 150);
    place (key, keyL, 62);
    place (mode, modeL, 112);
    place (structure, structureL, 140);
    place (voicing, voicingL, 120);
    {
        auto r = row.removeFromLeft (56);
        humanize.slider.setBounds (r.removeFromTop (36).withSizeKeepingCentre (36, 36));
        humanize.label.setBounds (r.withHeight (14).translated (-10, 0).withWidth (76));
        row.removeFromLeft (8);
    }
    rerollBtn.setBounds (row.removeFromRight (128).withTrimmedTop (18).withHeight (30));
    row.removeFromRight (8);
    generateBtn.setBounds (row.withTrimmedTop (18).withHeight (30));

    timeline.setBounds (18, 128, getWidth() - 36, 190);
    info.setBounds (22, 324, getWidth() - 44, 62);

    // panels
    const int py = 392, ph = getHeight() - py - 16;
    gPanel = { 18, py, 276, ph };
    bPanel = { gPanel.getRight() + 10, py, 222, ph };
    dPanel = { bPanel.getRight() + 10, py, 222, ph };
    sPanel = { dPanel.getRight() + 10, py, getWidth() - 18 - dPanel.getRight() - 10, ph };

    auto knobRow = [] (juce::Rectangle<int> panel, std::initializer_list<Knob*> knobs)
    {
        auto r = panel.reduced (12, 0).withTrimmedTop (42).withHeight (88);
        const int w = r.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
        {
            auto c = r.removeFromLeft (w);
            k->slider.setBounds (c.removeFromTop (66).withSizeKeepingCentre (62, 62));
            k->label.setBounds (c.withHeight (16));
        }
    };
    knobRow (gPanel, { &gDrive, &gTone, &gWidth, &gVol });
    knobRow (bPanel, { &bDrive, &bTone, &bVol });
    knobRow (dPanel, { &dPunch, &dRoom, &dVol });

    auto tile = [] (juce::Rectangle<int> panel) { return panel.reduced (14, 0).withTrimmedBottom (14).removeFromBottom (62).withY (panel.getBottom() - 14 - 62); };
    gTile.setBounds (tile (gPanel));
    bTile.setBounds (tile (bPanel));
    dTile.setBounds (tile (dPanel));
    songTile.setBounds (tile (sPanel));

    auto s = sPanel.reduced (14, 0).withTrimmedTop (40);
    {
        auto top = s.removeFromTop (88);
        auto k = top.removeFromLeft (70);
        master.slider.setBounds (k.removeFromTop (66).withSizeKeepingCentre (58, 58));
        master.label.setBounds (k.withHeight (16));
        top.removeFromLeft (6);
        midiInL.setBounds (top.removeFromTop (18));
        midiIn.setBounds (top.removeFromTop (28));
        top.removeFromTop (8);
        autoPlay.setBounds (top.removeFromTop (24));
    }
    s.removeFromTop (8);
    saveBtn.setBounds (s.removeFromTop (30));
}
