---
name: unslop
description: "Use when writing or cleaning up any prose surface in this repo: code comments, PR descriptions, commit messages, docs, the reply itself. Removes AI tells and filler. Trigger on 'unslop this', 'tighten this', 'clean up the writing', 'this reads like AI', 'rewrite the comment'. Use ONLY for writing quality, not for code logic."
---

# Unslop

Writing that reads like a machine produced it, and how to remove the tells. The rule that catches most of it: write clean as you go, never generate the bad sentence in the first place.

## The tells to remove

- **Hedging and filler.** "It is worth noting", "Additionally", "It is important to remember", "In conclusion". Delete them. The sentence was already saying the thing.
- **The long dash.** Banned outright. Join a filename to its description with a sentence, not a dash. Join a bold header to its body with a sentence, not a dash.
- **Colon as a mid-sentence connector.** A colon before a list is fine. A colon gluing two sentences together is a tell.
- **Emphatic emptiness.** "Very", "really", "crucially", "importantly", "effectively". If the emphasis is doing work, the word should be load-bearing; otherwise cut it.
- **Over-qualification.** "Potentially", "possibly", "in some cases", "often". One hedge per claim is honesty; three is noise. Say what you know and leave what you do not.
- **Obvious recaps.** Restating the previous sentence in different words.
- **The gush.** "This robust solution seamlessly integrates". Adjectives that assert quality instead of describing behavior.

## The shape to aim for

- Short declarative sentences. One thought per sentence.
- Specific over general. Name the file, the line, the behavior. "The shop menu's Accept button" beats "the user interface".
- Verbs over nouns. "The serializer restores town records" beats "restoration of town records is performed".
- The statement of fact, then the evidence, then the uncertainty. In that order, with nothing invented.

## Applies everywhere

Code comments, commit messages, PR descriptions, docs, and the reply you write. In this repo, an Enforce Script comment narrating a phase (`// phase 1: add cards`) is the classic offender: delete it, the assertion or log string is the doc. Keep a comment only for a non-obvious why the code cannot show.