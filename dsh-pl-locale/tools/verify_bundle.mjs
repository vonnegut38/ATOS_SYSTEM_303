/**
 * Verify the generated client bundle without a browser.
 *
 * The bundle is an ordinary script that calls `window.__ModuleLoader__.load`.
 * This test captures that definition, runs the factory with a require stub that
 * throws (the pack must have no module requests), applies the plugin to a fake
 * client context, and then checks the contribution against the extracted
 * catalog:
 *
 *   - the language definition is appended with an `en` fallback;
 *   - every registered namespace exists in the catalog;
 *   - every translated key exists in that namespace's shipped dictionary;
 *   - no value is empty and placeholders survive translation;
 *   - the fallback lookup returns Polish where translated and English where not.
 *
 * Usage: node tools/verify_bundle.mjs
 */

import { readFileSync, readdirSync } from 'node:fs'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
import vm from 'node:vm'

const root = join(dirname(fileURLToPath(import.meta.url)), '..')
const bundlePath = join(root, 'lib', 'client.js')

let captured
const sandbox = {
  window: {
    __ModuleLoader__: {
      load(definition) {
        captured = definition
      },
    },
  },
}

vm.runInNewContext(readFileSync(bundlePath, 'utf8'), sandbox, { filename: bundlePath })

const failures = []
const check = (condition, message) => {
  if (!condition) failures.push(message)
}

check(captured !== undefined, 'bundle did not call window.__ModuleLoader__.load')
if (captured === undefined) {
  console.error(failures.join('\n'))
  process.exit(1)
}

check(captured.id === 'dsh-locale-pl', `unexpected bundle id: ${captured.id}`)
check(typeof captured.factory === 'function', 'factory is not a function')

const requireStub = (request) => {
  throw new Error(`bundle requested module ${request}, but the pack must be import-free`)
}
const exports = captured.factory(requireStub)

check(Array.isArray(exports.inject), 'exports.inject is not an array')
check(exports.inject?.includes('locale'), 'exports.inject does not request the locale service')
check(typeof exports.apply === 'function', 'exports.apply is not a function')

// ── a minimal stand-in for the client context ────────────────────────────────
const languages = new Map([['en', { id: 'en', label: 'English' }]])
const dictionaries = new Map()
const disposers = []

const ctx = {
  effect(callback, label) {
    const dispose = callback()
    check(typeof dispose === 'function', `effect ${label} returned no disposer`)
    disposers.push({ label, dispose })
    return dispose
  },
  locale: {
    addLanguage(input) {
      check(typeof input?.id === 'string' && input.id !== '', 'addLanguage without an id')
      check(typeof input?.label === 'string' && input.label !== '', 'addLanguage without a label')
      check(languages.has(input.fallback), `addLanguage fallback ${input.fallback} is not registered`)
      check(!languages.has(input.id), `language ${input.id} registered twice`)
      languages.set(input.id, { ...input })
      return () => languages.delete(input.id)
    },
    register(namespace, locale, dictionary) {
      check(languages.has(locale), `register(${namespace}) before language ${locale} exists`)
      const key = `${namespace}\u0000${locale}`
      check(!dictionaries.has(key), `namespace ${namespace} registered twice for ${locale}`)
      dictionaries.set(key, dictionary)
      return () => dictionaries.delete(key)
    },
  },
}

exports.apply(ctx)

// ── catalog cross-check ──────────────────────────────────────────────────────
const catalogDir = join(root, 'catalog')
const catalog = new Map()
for (const file of readdirSync(catalogDir).filter((name) => name.endsWith('.json'))) {
  catalog.set(file.slice(0, -'.json'.length), JSON.parse(readFileSync(join(catalogDir, file), 'utf8')))
}

check(languages.has('pl'), 'the pl language was not registered')
check(languages.get('pl')?.fallback === 'en', 'pl does not fall back to en')
check(languages.get('pl')?.label === 'Polski', `unexpected pl label: ${languages.get('pl')?.label}`)

const placeholderPattern = /\{[^}]+\}/g
let keyCount = 0
for (const [key, dictionary] of dictionaries) {
  const [namespace, locale] = key.split('\u0000')
  check(locale === 'pl', `dictionary registered for locale ${locale}`)
  const source = catalog.get(namespace)
  check(source !== undefined, `namespace ${namespace} is absent from catalog/`)
  if (source === undefined) continue
  const english = source.en
  for (const [translationKey, value] of Object.entries(dictionary)) {
    keyCount += 1
    check(
      Object.hasOwn(english, translationKey),
      `${namespace}.${translationKey} does not exist in the shipped en dictionary`,
    )
    check(typeof value === 'string' && value.trim() !== '', `${namespace}.${translationKey} is empty`)
    const expected = (english[translationKey]?.match(placeholderPattern) ?? []).sort()
    const actual = (value.match(placeholderPattern) ?? []).sort()
    check(
      JSON.stringify(expected) === JSON.stringify(actual),
      `${namespace}.${translationKey}: placeholders ${JSON.stringify(actual)} != ${JSON.stringify(expected)}`,
    )
  }
}

// ── fallback lookup, mirroring the runtime chain ─────────────────────────────
const lookup = (namespace, key, locale) => {
  for (const candidate of locale === 'pl' ? ['pl', 'en'] : [locale]) {
    const dictionary = dictionaries.get(`${namespace}\u0000${candidate}`)
    if (dictionary && Object.hasOwn(dictionary, key)) return dictionary[key]
  }
  return catalog.get(namespace)?.en?.[key] ?? key
}

const sample = dictionaries.get('common\u0000pl')
check(sample !== undefined, 'no common dictionary registered')
if (sample) {
  const [firstKey] = Object.keys(sample)
  check(lookup('common', firstKey, 'pl') === sample[firstKey], 'translated key does not resolve')
}
const untranslated = Object.keys(catalog.get('trajectory').en)[0]
check(
  lookup('trajectory', untranslated, 'pl') === catalog.get('trajectory').en[untranslated],
  'untranslated namespace does not fall back to English',
)

check(disposers.length >= 2, 'the plugin registered no disposable effects')

// ── report ───────────────────────────────────────────────────────────────────
if (failures.length > 0) {
  console.error(`FAIL — ${failures.length} problem(s):`)
  for (const failure of failures) console.error(' -', failure)
  process.exit(1)
}

console.log('OK  bundle id           dsh-locale-pl')
console.log(`OK  languages           ${[...languages.keys()].join(', ')}`)
console.log(`OK  namespaces          ${[...dictionaries.keys()].filter((k) => k.endsWith('pl')).length}`)
console.log(`OK  translated keys     ${keyCount}`)
console.log(`OK  effects             ${disposers.length}`)
console.log('OK  fallback            pl → en, untranslated keys resolve to English')
