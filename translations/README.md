# HDEPanel Translations

This directory contains translation files for HDEPanel in multiple languages.

## Supported Languages

- **English (en)**: hdepanel_en.ts
- **Dutch (nl)**: hdepanel_nl.ts
- **Arabic (ar)**: hdepanel_ar.ts

## How to Update Translations

### 1. Extract translatable strings from source code

Run this command from the project root:

```bash
lupdate hdepanel.pro
```

This will scan all source files and update the `.ts` files with new translatable strings.

### 2. Translate the strings

Open the `.ts` files with Qt Linguist:

```bash
linguist translations/hdepanel_nl.ts
```

Or edit them manually in a text editor.

### 3. Compile translations to binary format

After translating, compile the `.ts` files to `.qm` (binary) format:

```bash
lrelease hdepanel.pro
```

Or compile individual files:

```bash
lrelease translations/hdepanel_nl.ts -qm translations/hdepanel_nl.qm
lrelease translations/hdepanel_ar.ts -qm translations/hdepanel_ar.qm
```

### 4. Test translations

Set your system locale or use the LANGUAGE environment variable:

```bash
LANGUAGE=nl ./hdepanel-launch.sh  # Dutch
LANGUAGE=ar ./hdepanel-launch.sh  # Arabic
LANGUAGE=en ./hdepanel-launch.sh  # English
```

## Adding New Languages

1. Add a new `.ts` file in the `TRANSLATIONS` variable in `hdepanel.pro`:
   ```qmake
   TRANSLATIONS += translations/hdepanel_fr.ts
   ```

2. Run `lupdate` to create the file:
   ```bash
   lupdate hdepanel.pro
   ```

3. Translate the new file and compile it with `lrelease`.

## Translation Guidelines

- Keep translations concise - panel space is limited
- Test translations in the actual UI to ensure they fit
- For Arabic, ensure proper RTL (right-to-left) display
- Use formal language for menu items and tooltips
- Maintain consistency across all UI elements

