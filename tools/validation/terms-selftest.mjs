// Red-proof for validate-terms (#958): each failure class is caught, a clean table is not.
import { parseTable, parseTermsConf } from "../lib/terms.mjs";
import { check } from "./validate-terms.mjs";

const conf = `LF_TermsConfig {
 m_aTerms {
  LF_TermRecord "{0000000000000001}" {
   m_sId "police"
   m_aForms {
    LF_TermForm "{0000000000000002}" {
     m_sForm "singular"
     m_sValue "#LF-Term_Police"
    }
   }
  }
  LF_TermRecord "{0000000000000003}" {
   m_sId "currency"
   m_aForms {
    LF_TermForm "{0000000000000004}" {
     m_sForm "singular"
     m_sValue "#a"
    }
    LF_TermForm "{0000000000000005}" {
     m_sForm "sym"
     m_sValue "#b"
    }
   }
  }
 }
}
`;
const terms = {};
for (const [id, forms] of Object.entries(parseTermsConf(conf))) terms[id] = { forms };
const item = (id, en, de, comment) => `CustomStringTableItem "{0000000000000009}" {\n Id "${id}"\n Target_en_us "${en}"\n${de ? ` Target_de_de "${de}"\n` : ""}${comment ? ` Comment "${comment}"\n` : ""}}\n`;
const table = (...items) => parseTable(items.join(""));
const run = (core, baseline = {}, packs = []) => check(table(...core), packs, terms, baseline).failures;
let failed = 0;
const expect = (name, failures, pattern) => {
  const ok = pattern ? failures.some((f) => pattern.test(f)) : failures.length === 0;
  if (!ok) { failed++; console.log(`FAIL terms-selftest: ${name}; got ${JSON.stringify(failures)}`); }
};

expect("a clean table passes", run([item("A", "Call the {police}.", "Rufen Sie die {police}"), item("B", "Pay {currency:sym}%1 {Currency}")]));
expect("a terms file parses to forms", Object.keys(terms.currency.forms).includes("sym") ? [] : ["no sym"]);
expect("an unknown term is caught", run([item("A", "Call the {sheriff}.")]), /term "sheriff"/);
expect("an unknown term in German is caught", run([item("A", "ok", "Der {sheriff}")]), /\(de_de\).*term "sheriff"/);
expect("an unknown form is caught", run([item("A", "{police:acc}")]), /form "acc"/);
expect("a capital names the lower-case term", run([item("A", "{Police} first")]));
expect("pl is a form only when the term has plural", run([item("A", "{police:pl}")]), /form "plural"/);
expect("a pack table is held to the same rule", run([item("A", "x")], {}, [["pack", table(item("P", "{ghost}"))]]), /pack: P/);
expect("a GUID in braces is not a placeholder", run([item("A", "{9B5BB216CC7FF18E}")]));
expect("a new key that hard-codes police fails", run([item("A", "Call the police.")]), /hard-codes police/);
expect("a new key that hard-codes a dollar sign fails", run([item("A", "Pay $%1")]), /hard-codes currency/);
expect("a baseline key passes", run([item("A", "Call the police.")], { A: ["police"] }));
expect("a baseline key that was migrated must leave the list", run([item("A", "Call the {police}.")], { A: ["police"] }), /remove it from the baseline/);
expect("a baseline key may not pick up another term", run([item("A", "police in town")], { A: ["police"] }), /now hard-codes town/);
expect("the placeholder itself is not a hard-coded word", run([item("A", "The {currency} {police}")]));

expect("an all-caps placeholder names the term", run([item("A", "{POLICE} TICKET")]));
expect("an all-caps placeholder of an unknown term is caught", run([item("A", "{SHERIFF} TICKET")]), /term "sheriff"/);
expect("a voiced line may keep the recorded word", run([item("A", "Call the police.", "", "Dialogue caption (voiced; tools/assets/dialogue/clerk)")]));
expect("a term's own words are not hard-coded text", run([item("LF-Term_Police", "police")]));

if (failed) process.exit(1);
console.log("terms-selftest: OK");
