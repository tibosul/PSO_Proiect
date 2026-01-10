# Documentație DHCP v4 și v6 - LaTeX

Această documentație prezintă implementarea și analiza sistemului de servere DHCP v4 și v6 din acest proiect.

## Fișiere

- **dhcp_documentation.tex** - Documentul LaTeX principal (format IEEE)
- **README.md** - Acest fișier cu instrucțiuni

## Caracteristici

- **Format**: IEEE Conference template
- **Limbă**: Română cu diacritice complete (ă, â, î, ș, ț)
- **Lungime**: ~8 pagini de conținut
- **Capitole**: 7 capitole principale + bibliografie

## Structura Documentului

1. **Introducere** - Context, motivație, domeniu de aplicare, obiective
2. **Related Work** - Studiul standardelor RFC, implementări existente, aspecte teoretice
3. **Arhitectura și Designul Sistemului** - Componente, pattern-uri, fluxuri de date, diagrame
4. **Implementare** - Tehnologii, structură proiect, protocoale, formate date, cod semnificativ
5. **Testare și Validare** - Metodologii, scenarii, rezultate, cerințe non-funcționale
6. **Concluzii și Perspective** - Sinteză, dificultăți, analiză critică, extensii viitoare
7. **Bibliografie** - 13 referințe (RFC-uri, cărți, articole)

## Compilare în Overleaf

### Pasul 1: Crearea proiectului
1. Accesați [Overleaf](https://www.overleaf.com)
2. Creați un nou proiect: **New Project** → **Blank Project**
3. Denumiți proiectul: "DHCP v4 și v6 Documentation"

### Pasul 2: Încărcarea fișierului
1. Ștergeți conținutul implicit din `main.tex`
2. Copiați întregul conținut din `dhcp_documentation.tex`
3. Lipiți în `main.tex` din Overleaf

### Pasul 3: Configurare compilator
1. Click pe meniul din stânga sus (☰)
2. Settings → Compiler
3. Selectați: **pdfLaTeX** (recomandat) sau **XeLaTeX**

### Pasul 4: Compilare
1. Click pe butonul **Recompile** (sau Ctrl/Cmd + S)
2. Așteptați 10-20 secunde pentru compilare
3. PDF-ul va apărea în panoul din dreapta

## Compilare Locală (Linux/Mac)

Dacă doriți să compilați local, aveți nevoie de o distribuție LaTeX completă.

### Instalare TeXLive (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install texlive-latex-extra texlive-lang-european
```

### Instalare TeXLive (macOS)
```bash
brew install --cask mactex
```

### Compilare
```bash
cd documentation
pdflatex dhcp_documentation.tex
bibtex dhcp_documentation
pdflatex dhcp_documentation.tex
pdflatex dhcp_documentation.tex
```

Notă: Sunt necesare 3 rulări ale `pdflatex` pentru referințe corecte și bibliografie.

## Pachete LaTeX Necesare

Documentul folosește următoarele pachete (incluse în TeXLive standard):

- `inputenc` - Suport UTF-8 pentru diacritice
- `babel` - Suport limba română
- `cite` - Gestionare citări bibliografice
- `amsmath, amssymb, amsfonts` - Simboluri matematice
- `graphicx` - Suport imagini (opțional)
- `listings` - Afișare cod sursă
- `xcolor` - Culori pentru syntax highlighting
- `url` - Formatare URL-uri

Toate aceste pachete sunt disponibile implicit în Overleaf.

## Personalizare

### Modificarea autorului
Căutați secțiunea `\author` și editați:
```latex
\author{\IEEEauthorblockN{Numele Dvs.}
\IEEEauthorblockA{\textit{Departamentul} \\
\textit{Universitatea}\\
Oraș, Țară \\
email@example.com}
}
```

### Adăugarea de imagini
Pentru a adăuga diagrame sau capturi de ecran:

1. Încărcați imaginea în Overleaf (PNG, JPG sau PDF)
2. Adăugați în document:
```latex
\begin{figure}[htbp]
\centerline{\includegraphics[width=0.8\columnwidth]{nume_imagine.png}}
\caption{Descrierea imaginii}
\label{fig:eticheta}
\end{figure}
```

3. Referențiați în text: `\ref{fig:eticheta}`

### Adăugarea de tabele
Exemplu tabel pentru rezultate:
```latex
\begin{table}[htbp]
\caption{Rezultate Testare Performanță}
\begin{center}
\begin{tabular}{|l|c|c|}
\hline
\textbf{Metric} & \textbf{DHCPv4} & \textbf{DHCPv6} \\
\hline
Timp răspuns (ms) & 2.3 & 3.1 \\
Throughput (req/s) & 500 & 450 \\
Memorie (MB) & 15 & 18 \\
\hline
\end{tabular}
\label{tab:performance}
\end{center}
\end{table}
```

## Conținut Documentație

### Secțiuni Principale

**Introducere (1 pagină)**
- Context general DHCP
- Motivație implementare duală v4/v6
- Domenii de aplicare practice
- Obiective clare și măsurabile

**Related Work (1 pagină)**
- RFC 2131 (DHCPv4) și RFC 8415 (DHCPv6)
- Implementări existente: ISC DHCP, dnsmasq, Kea
- Aspecte teoretice și comparații v4 vs v6
- Securitate și performanță

**Arhitectură (1.5 pagini)**
- Componente majore ale sistemului
- Pattern-uri arhitecturale (Layered, Thread Pool, Factory)
- Fluxuri de date DORA (v4) și SARR (v6)
- Diagrame de interacțiune

**Implementare (2 pagini)**
- Tehnologii: C11, BSD sockets, POSIX threads
- Structura detaliată a proiectului
- Protocoale de comunicare (UDP, porturi, adrese)
- Formate de date (structuri, baze de date)
- Fragmente de cod semnificative
- Aspecte tehnice: concurență, memorie, optimizări

**Testare (1.5 pagini)**
- Metodologii: unitară, integrare, sistem, performanță
- 6 scenarii de test detaliate (loopback, LAN, VM, Docker, etc)
- Rezultate măsurate: acoperire, performanță, stabilitate
- Cerințe non-funcționale: disponibilitate, scalabilitate

**Concluzii (1 pagină)**
- Sinteză rezultate și realizări
- Dificultăți întâmpinate și soluții
- Analiză critică (puncte tari și limitări)
- Perspective de dezvoltare viitoare (15+ idei)

**Bibliografie**
- 13 referințe academice și tehnice
- RFC-uri oficiale
- Cărți de specialitate (Tanenbaum, Stevens)
- Resurse online verificate

## Verificare Document

### Checklist Completitudine
- [x] Template IEEE Conference folosit
- [x] Limba română cu diacritice corecte
- [x] Toate cele 7 capitole principale
- [x] Abstract și keywords
- [x] ~8 pagini de conținut (estimat 7-9 pagini compilat)
- [x] Bibliografie cu 13+ referințe
- [x] Cod sursă formatat cu `listings`
- [x] Structură ierarhică cu subsecțiuni
- [x] Fără DNS (focusat doar pe DHCP)

### Verificare Diacritice
Documentul folosește corect următoarele caractere românești:
- ă: și, că, până, după
- â: între, împreună, următoarele
- î: în, înțelegere, îmbunătățire
- ș: poate, funcționalitate, următoarele
- ț: rețele, funcționalitate, soluții

## Probleme Cunoscute și Soluții

### Eroare: "Package babel Error: Unknown language romanian"
**Soluție**: În Overleaf, verificați că aveți `texlive-lang-european` inclus. Alternativ, schimbați cu:
```latex
\usepackage[english]{babel}
```

### Avertisment: "Overfull \hbox"
**Soluție**: Normal pentru cod lung. Ignorați sau ajustați `\lstset{breaklines=true}`.

### PDF nu afișează diacritice
**Soluție**: Asigurați-vă că aveți:
```latex
\usepackage[utf8]{inputenc}
\usepackage[romanian]{babel}
```

## Export și Distribuție

### Din Overleaf
1. **PDF**: Click **Download PDF** (icon descărcare)
2. **Sursă**: **Menu** → **Download** → **Source**

### Versiuni
- **v1.0** (2026-01-10): Versiune inițială completă

## Licență

Documentația este parte din proiectul PSO_Proiect și urmează aceeași licență ca și codul sursă.

## Contact

Pentru întrebări sau sugestii legate de documentație, deschideți un issue în repository-ul GitHub.

---

**Nota**: Acest document este optimizat pentru Overleaf și nu necesită configurări suplimentare. Simplu copy-paste și compilare.
