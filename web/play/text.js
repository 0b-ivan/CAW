// Every visible text and accessibility label of the game page, German and English.
const texts = {
  de: {
    title: 'C/A/W · Session {0}',
    connecting: 'VERBINDE', connected: 'VERBUNDEN', offline: 'OFFLINE',
    pause: '[ PAUSE ]', menu: '[ ≡ ]', menuLabel: 'Menü',
    session: 'SESSION', sessionCode: 'SESSION {0}', lobby: 'LOBBY',
    round: 'RUNDE {0}/{1}', juror: 'JUROR {0}', rando: 'RANDO', theHost: 'DER HOST',

    connectingTitle: 'VERBINDE MIT DER SESSION',
    missingTitle: 'SESSION NICHT GEFUNDEN',
    missingText: 'Der Code ist falsch oder die Session ist abgelaufen.',
    home: '[ ZUR STARTSEITE ]',

    yourName: 'DEIN NAME', namePlaceholder: 'NAME', join: '[ BEITRETEN ]',
    joinHint: '3–8 SPIELER', playAlong: '[ MITSPIELEN ]', hostName: 'MITSPIELEN ALS',
    nameTooLong: 'NAME ZU LANG (MAX. 23 ZEICHEN)',
    seatExpired: 'DEIN PLATZ IST ABGELAUFEN. NEU BEITRETEN.',

    players: 'SPIELER · {0} VON MIND. 3', waitForHost: 'WARTE AUF HOST',
    start: '[ START ]', needPlayers: 'NOCH {0} SPIELER', ready: 'BEREIT',
    copyLink: '[ LINK KOPIEREN ]', share: '[ TEILEN ]', copied: 'LINK KOPIERT',
    copyFailed: 'KOPIEREN NICHT MÖGLICH', qrLabel: 'QR-Code für den Session-Link',
    inviteAddressLabel: 'ADRESSE FÜR MITSPIELER', inviteAddressNeeded: 'LAN-IP ODER ERREICHBARE URL EINTRAGEN',
    editInvite: 'EINLADUNGSADRESSE BEARBEITEN / SCHLIESSEN',
    inviteAddressInvalid: 'BITTE VOLLSTÄNDIGE HTTP(S)-ADRESSE OHNE PFAD EINGEBEN',
    offlineMark: ' · OFFLINE', youMark: ' · DU', hostMark: ' · HOST',
    hostRole: 'HOST',

    pick: 'PICK {0}', handLabel: 'Deine Karten',
    handTitle: 'DEINE KARTEN ({0})',
    promptLabel: 'Frage, scrollbar',
    selected: '{0}/{1} GEWÄHLT', swap: '[ ABWERFEN ]', swapped: '[ GETAUSCHT ]',
    submit: '[ ABGEBEN ]', slot: '<{0}/{1}>', slotLabel: 'Position {0} von {1}: {2}',

    submittedTitle: 'ABGEGEBEN · {0}/{1}', youAreJuror: 'DU BIST JUROR',
    answersTitle: 'ANTWORTEN · {0}/{1}', waitAnswers: 'WARTE AUF DIE ANTWORTEN',
    reveal: '[ AUFDECKEN ]',

    jurorDecides: 'JUROR {0} ENTSCHEIDET', chooseWinner: 'WÄHLE DEN BESTEN SATZ',
    nothingChosen: 'NICHTS GEWÄHLT', chosen: '[{0}] GEWÄHLT', confirm: '[ BESTÄTIGEN ]',
    playsLabel: 'Anonyme Antworten',

    revealTitle: 'AUFLÖSUNG · RUNDE {0}/{1}', winner: 'GEWINNER: {0}',
    youWon: 'DU HAST DIE RUNDE GEWONNEN', roundAborted: 'RUNDE ABGEBROCHEN',
    nextRound: '[ NÄCHSTE RUNDE ]', toResult: '[ ZUM ENDSTAND ]',
    nextIn: 'NÄCHSTE RUNDE IN {0}', hostContinues: 'DER HOST MACHT WEITER',
    scoresLabel: 'Punktestand', points: '{0}',

    gameOver: 'SPIELENDE', wins: 'GEWINNT', winTie: 'GEWINNEN',
    votes: '{0} VON {1} WOLLEN NOCHMAL', again: '[ NOCHMAL ]', againDone: '[ DABEI ]',
    rematch: '[ REMATCH ]', standingsLabel: 'Endstand',

    paused: 'PAUSE', pausedBy: 'GESETZT VON {0}', resume: '[ WEITER ]',
    pausedOther: 'NUR {0} ODER DER HOST KÖNNEN WEITERMACHEN.',
    pausedMine: 'NUR DU ODER DER HOST KÖNNEN WEITERMACHEN.',
    waiting: 'WARTEN', waitingFor: 'WARTET AUF {0}',
    blockedText: '{0} IST OFFLINE. DER HOST ENTSCHEIDET.', continueWithout: '[ OHNE {0} WEITER ]',

    removedTitle: 'DU WURDEST ENTFERNT', removedText: 'In der nächsten Lobby kannst du wieder beitreten.',
    nextLobby: '[ NÄCHSTE LOBBY ]',
    replacedTitle: 'IN EINEM ANDEREN TAB GEÖFFNET',
    replacedText: 'Dein Platz läuft jetzt in einem anderen Fenster.', playHere: '[ HIER WEITERSPIELEN ]',

    groupGame: 'SPIEL', groupHost: 'HOST', groupSession: 'SESSION',
    close: '[ SCHLIESSEN ]', backRow: 'ZURÜCK', startRow: 'START', rematchRow: 'REMATCH',
    language: 'SPRACHE', rules: 'SPIELREGELN', playersMenu: 'SPIELER', endGame: 'SPIEL BEENDEN',
    abortRound: 'RUNDE ABBRECHEN', addTime: '+ZEIT', revealNow: 'JETZT AUFDECKEN',
    hostPause: 'PAUSE', hostResume: 'WEITER', tapAgain: 'NOCHMAL TIPPEN',
    remove: 'ENTFERNEN', copyLinkRow: 'LINK KOPIEREN', shareRow: 'TEILEN', homeRow: 'STARTSEITE',
    handRule: 'HANDKARTEN', rounds: 'RUNDEN', answerClock: 'ANTWORT-UHR', extension: 'VERLÄNGERUNG',
    jurorClock: 'JUROR-UHR', autoNext: 'AUTO-WEITER', randoRule: 'RANDO',
    off: 'AUS', on: 'AN', seconds: '{0} S', fewer: 'Weniger', more: 'Mehr',
    roundsLocked: 'RUNDEN NUR IN DER LOBBY',
    handAndRoundsLocked: 'HANDKARTEN UND RUNDEN NUR IN DER LOBBY',
    noPlayers: 'NOCH KEINE SPIELER',
    customCards: 'EIGENE KARTEN', customLoading: 'LADE EIGENE KARTEN …',
    customHint: 'NUR FÜR DIESE SESSION. ZUM BEHALTEN JSON EXPORTIEREN. ALLE ENTFERNEN UND SPEICHERN LÖSCHT DAS PACK.',
    customLobbyOnly: 'KARTEN NUR IN DER LOBBY BEARBEITEN UND SPEICHERN.',
    customPrompts: 'SCHWARZE FRAGEKARTEN', customAnswers: 'WEISSE ANTWORTKARTEN',
    customBlanks: '___ IST EINE LÜCKE. 1–3 LÜCKEN JE FRAGE; BEI DREI ZWEI ZUSATZKARTEN.',
    customPrompt: 'FRAGE {0}', customAnswer: 'ANTWORT {0}',
    customRemove: '[ ENTFERNEN ]', customAddPrompt: '[ FRAGE HINZUFÜGEN ]',
    customAddAnswer: '[ ANTWORT HINZUFÜGEN ]', customSave: '[ SPEICHERN & AKTIVIEREN ]',
    customExport: '[ JSON EXPORTIEREN ]', customImport: '[ JSON IMPORTIEREN ]',
    customSaved: 'KARTEN GESPEICHERT UND AKTIV.', customCleared: 'EIGENES PACK ENTFERNT.', customImported: 'DATEI IMPORTIERT. ZUM AKTIVIEREN SPEICHERN.',
    customInvalid: 'MINDESTENS EINE KARTE, MAXIMAL 300 UTF-8-BYTES JE KARTE.',
    customUnavailable: 'NUR IN DER LOBBY MIT PLATZ FÜR EIN ACHTES PACK.',
    customSaveFailed: 'KARTEN UNGÜLTIG ODER SERVER NICHT ERREICHBAR.',
    customLoadFailed: 'KARTEN KONNTEN NICHT GELADEN WERDEN.', customImportFailed: 'UNGÜLTIGE JSON-DATEI.',
    proposalMenu: 'KARTE VORSCHLAGEN', proposalsMenu: 'KARTENVORSCHLÄGE',
    proposalHint: 'EINE EIGENE KARTE FÜR DIE LOBBY EINREICHEN. DER HOST MUSS SIE ANNEHMEN. ___ MARKIERT EINE LÜCKE.',
    proposalType: 'KARTENTYP', proposalPrompt: 'FRAGE', proposalAnswer: 'ANTWORT',
    proposalText: 'KARTENTEXT', proposalSend: '[ VORSCHLAGEN ]',
    proposalInvalid: 'KARTENTEXT ERFORDERLICH (MAX. 300 UTF-8-BYTES).',
    proposalSent: 'DEM HOST VORGESCHLAGEN.', proposalUnavailable: 'NUR IN DER LOBBY; MAXIMAL 3 PRO SPIELER UND 24 IM RAUM.',
    proposalFailed: 'VORSCHLAG NICHT ÜBERMITTELT.', proposalReviewHint: 'NUR FREIGEGEBENE KARTEN KOMMEN INS SPIEL.',
    proposalRefresh: '[ AKTUALISIEREN ]', proposalLoading: 'LADE VORSCHLÄGE …',
    proposalEmpty: 'KEINE OFFENEN VORSCHLÄGE.', proposalBy: 'VON {0}',
    proposalAccept: '[ ANNEHMEN ]', proposalReject: '[ ABLEHNEN ]',
    proposalAccepted: 'KARTE ÜBERNOMMEN.', proposalRejected: 'VORSCHLAG ABGELEHNT.',
    proposalReviewFailed: 'NICHT MÖGLICH. DECK VOLL ODER LOBBY BEENDET.',
    proposalLoadFailed: 'VORSCHLÄGE KONNTEN NICHT GELADEN WERDEN.',
    packsMenu: 'KARTEN-PACKS', packCards: '{0} / {1}', packsLobbyOnly: 'PACKS NUR IN DER LOBBY',
    packsLoading: 'LADE PACKS …', packsHint: 'FRAGEN / ANTWORTEN. MINDESTENS EIN PACK, HÖCHSTENS ACHT.',
    credits: 'KARTEN: {0}',
    lockLobby: 'LOBBY SPERREN', unlockLobby: 'LOBBY ÖFFNEN', lobbyLocked: 'LOBBY GESPERRT',
    fame: 'RUHMESHALLE', fameStats: '{0} SIEGE · {1} RUNDEN · {2} SPIELE', fameEmpty: 'NOCH KEINE EINTRÄGE',
    fameLines: 'LETZTE SIEGERSÄTZE', fameLoading: 'LADE …', matchLines: 'SIEGERSÄTZE DER PARTIE',
    pageOf: '{0}/{1}', previous: 'Vorherige Seite', nextPage: 'Nächste Seite',
    bigScreen: 'BIG SCREEN', bigScreenButton: '[ BIG SCREEN ]',
    bigScreenText: 'Am Fernseher oder Laptop öffnen: zeigt Frage, Uhr und Sätze für alle.',
    copyScreenLink: '[ LINK KOPIEREN ]', openScreen: '[ ÖFFNEN ]', screenQrLabel: 'QR-Code für den Big-Screen-Link',
    screenTitle: 'C/A/W · Big Screen {0}', screenJoin: 'HANDY SCANNEN ODER CODE EINGEBEN',
    submittedBar: 'ABGEGEBEN', fullscreen: '[ VOLLBILD ]',

    errors: {
      rejected: 'NICHT MÖGLICH', not_host: 'NUR DER HOST', already_joined: 'BEREITS BEIGETRETEN',
      game_running: 'SPIEL LÄUFT BEREITS · NÄCHSTE LOBBY ABWARTEN', session_full: 'SESSION VOLL',
      locked: 'LOBBY GESPERRT · FRAG DEN HOST', rate_limited: 'ZU VIELE VERSUCHE · KURZ WARTEN',
    },
  },
  en: {
    title: 'C/A/W · Session {0}',
    connecting: 'CONNECTING', connected: 'CONNECTED', offline: 'OFFLINE',
    pause: '[ PAUSE ]', menu: '[ ≡ ]', menuLabel: 'Menu',
    session: 'SESSION', sessionCode: 'SESSION {0}', lobby: 'LOBBY',
    round: 'ROUND {0}/{1}', juror: 'JUDGE {0}', rando: 'RANDO', theHost: 'THE HOST',

    connectingTitle: 'CONNECTING TO THE SESSION',
    missingTitle: 'SESSION NOT FOUND',
    missingText: 'The code is wrong or the session has expired.',
    home: '[ TO THE START PAGE ]',

    yourName: 'YOUR NAME', namePlaceholder: 'NAME', join: '[ JOIN ]',
    joinHint: '3–8 PLAYERS', playAlong: '[ PLAY ALONG ]', hostName: 'PLAY ALONG AS',
    nameTooLong: 'NAME TOO LONG (MAX 23 CHARACTERS)',
    seatExpired: 'YOUR SEAT HAS EXPIRED. JOIN AGAIN.',

    players: 'PLAYERS · {0} OF MIN 3', waitForHost: 'WAITING FOR THE HOST',
    start: '[ START ]', needPlayers: '{0} MORE PLAYERS', ready: 'READY',
    copyLink: '[ COPY LINK ]', share: '[ SHARE ]', copied: 'LINK COPIED',
    copyFailed: 'COPYING NOT POSSIBLE', qrLabel: 'QR code for the session link',
    inviteAddressLabel: 'ADDRESS FOR PLAYERS', inviteAddressNeeded: 'ENTER REACHABLE LAN IP OR URL',
    editInvite: 'EDIT / CLOSE INVITE ADDRESS',
    inviteAddressInvalid: 'ENTER A FULL HTTP(S) ADDRESS WITHOUT A PATH',
    offlineMark: ' · OFFLINE', youMark: ' · YOU', hostMark: ' · HOST',
    hostRole: 'HOST',

    pick: 'PICK {0}', handLabel: 'Your cards',
    handTitle: 'YOUR CARDS ({0})',
    promptLabel: 'Prompt, scrollable',
    selected: '{0}/{1} SELECTED', swap: '[ DISCARD ]', swapped: '[ SWAPPED ]',
    submit: '[ SUBMIT ]', slot: '<{0}/{1}>', slotLabel: 'Position {0} of {1}: {2}',

    submittedTitle: 'SUBMITTED · {0}/{1}', youAreJuror: 'YOU ARE THE JUDGE',
    answersTitle: 'ANSWERS · {0}/{1}', waitAnswers: 'WAITING FOR THE ANSWERS',
    reveal: '[ REVEAL ]',

    jurorDecides: 'JUDGE {0} IS DECIDING', chooseWinner: 'PICK THE BEST SENTENCE',
    nothingChosen: 'NOTHING SELECTED', chosen: '[{0}] SELECTED', confirm: '[ CONFIRM ]',
    playsLabel: 'Anonymous answers',

    revealTitle: 'REVEAL · ROUND {0}/{1}', winner: 'WINNER: {0}',
    youWon: 'YOU WON THE ROUND', roundAborted: 'ROUND ABORTED',
    nextRound: '[ NEXT ROUND ]', toResult: '[ FINAL SCORES ]',
    nextIn: 'NEXT ROUND IN {0}', hostContinues: 'THE HOST CONTINUES',
    scoresLabel: 'Scores', points: '{0}',

    gameOver: 'GAME OVER', wins: 'WINS', winTie: 'WIN',
    votes: '{0} OF {1} WANT TO PLAY AGAIN', again: '[ AGAIN ]', againDone: '[ IN ]',
    rematch: '[ REMATCH ]', standingsLabel: 'Final standings',

    paused: 'PAUSED', pausedBy: 'BY {0}', resume: '[ RESUME ]',
    pausedOther: 'ONLY {0} OR THE HOST CAN RESUME.',
    pausedMine: 'ONLY YOU OR THE HOST CAN RESUME.',
    waiting: 'WAITING', waitingFor: 'WAITING FOR {0}',
    blockedText: '{0} IS OFFLINE. THE HOST DECIDES.', continueWithout: '[ GO ON WITHOUT {0} ]',

    removedTitle: 'YOU WERE REMOVED', removedText: 'You can join again in the next lobby.',
    nextLobby: '[ NEXT LOBBY ]',
    replacedTitle: 'OPENED IN ANOTHER TAB',
    replacedText: 'Your seat now runs in another window.', playHere: '[ PLAY HERE ]',

    groupGame: 'GAME', groupHost: 'HOST', groupSession: 'SESSION',
    close: '[ CLOSE ]', backRow: 'BACK', startRow: 'START', rematchRow: 'REMATCH',
    language: 'LANGUAGE', rules: 'RULES', playersMenu: 'PLAYERS', endGame: 'END GAME',
    abortRound: 'ABORT ROUND', addTime: '+TIME', revealNow: 'REVEAL NOW',
    hostPause: 'PAUSE', hostResume: 'RESUME', tapAgain: 'TAP AGAIN',
    remove: 'REMOVE', copyLinkRow: 'COPY LINK', shareRow: 'SHARE', homeRow: 'START PAGE',
    handRule: 'HAND CARDS', rounds: 'ROUNDS', answerClock: 'ANSWER CLOCK', extension: 'EXTENSION',
    jurorClock: 'JUDGE CLOCK', autoNext: 'AUTO NEXT', randoRule: 'RANDO',
    off: 'OFF', on: 'ON', seconds: '{0} S', fewer: 'Less', more: 'More',
    roundsLocked: 'ROUNDS ONLY IN THE LOBBY',
    handAndRoundsLocked: 'HAND CARDS AND ROUNDS ONLY IN THE LOBBY',
    noPlayers: 'NO PLAYERS YET',
    customCards: 'MY CARDS', customLoading: 'LOADING YOUR CARDS …',
    customHint: 'ONLY IN THIS SESSION. EXPORT JSON TO KEEP THEM. REMOVE ALL CARDS AND SAVE TO DELETE THE PACK.',
    customLobbyOnly: 'CARDS CAN ONLY BE EDITED AND SAVED IN THE LOBBY.',
    customPrompts: 'BLACK QUESTION CARDS', customAnswers: 'WHITE ANSWER CARDS',
    customBlanks: 'USE ___ FOR A BLANK. 1–3 BLANKS PER QUESTION; THREE DRAW TWO EXTRA CARDS.',
    customPrompt: 'QUESTION {0}', customAnswer: 'ANSWER {0}',
    customRemove: '[ REMOVE ]', customAddPrompt: '[ ADD QUESTION ]',
    customAddAnswer: '[ ADD ANSWER ]', customSave: '[ SAVE & ACTIVATE ]',
    customExport: '[ EXPORT JSON ]', customImport: '[ IMPORT JSON ]',
    customSaved: 'CARDS SAVED AND ACTIVATED.', customCleared: 'CUSTOM PACK REMOVED.', customImported: 'FILE IMPORTED. SAVE TO ACTIVATE.',
    customInvalid: 'ENTER AT LEAST ONE CARD; MAXIMUM 300 UTF-8 BYTES PER CARD.',
    customUnavailable: 'ONLY IN THE LOBBY, WITH SPACE FOR AN EIGHTH PACK.',
    customSaveFailed: 'CARDS INVALID OR SERVER UNREACHABLE.',
    customLoadFailed: 'CARDS COULD NOT BE LOADED.', customImportFailed: 'INVALID JSON FILE.',
    proposalMenu: 'SUGGEST A CARD', proposalsMenu: 'CARD SUGGESTIONS',
    proposalHint: 'SUGGEST A CARD IN THE LOBBY. THE HOST MUST APPROVE IT. ___ MARKS A BLANK.',
    proposalType: 'CARD TYPE', proposalPrompt: 'QUESTION', proposalAnswer: 'ANSWER',
    proposalText: 'CARD TEXT', proposalSend: '[ SUGGEST ]',
    proposalInvalid: 'CARD TEXT REQUIRED (MAX. 300 UTF-8 BYTES).',
    proposalSent: 'SENT TO THE HOST.', proposalUnavailable: 'LOBBY ONLY; MAXIMUM 3 PER PLAYER AND 24 PER ROOM.',
    proposalFailed: 'COULD NOT SEND YOUR SUGGESTION.', proposalReviewHint: 'ONLY APPROVED CARDS ENTER THE GAME.',
    proposalRefresh: '[ REFRESH ]', proposalLoading: 'LOADING SUGGESTIONS …',
    proposalEmpty: 'NO PENDING SUGGESTIONS.', proposalBy: 'FROM {0}',
    proposalAccept: '[ ACCEPT ]', proposalReject: '[ REJECT ]',
    proposalAccepted: 'CARD ADDED.', proposalRejected: 'SUGGESTION REJECTED.',
    proposalReviewFailed: 'UNAVAILABLE. DECK FULL OR LOBBY CLOSED.',
    proposalLoadFailed: 'COULD NOT LOAD SUGGESTIONS.',
    packsMenu: 'CARD PACKS', packCards: '{0} / {1}', packsLobbyOnly: 'PACKS ONLY IN THE LOBBY',
    packsLoading: 'LOADING PACKS …', packsHint: 'PROMPTS / ANSWERS. AT LEAST ONE PACK, AT MOST EIGHT.',
    credits: 'CARDS: {0}',
    lockLobby: 'LOCK LOBBY', unlockLobby: 'OPEN LOBBY', lobbyLocked: 'LOBBY LOCKED',
    fame: 'WALL OF FAME', fameStats: '{0} WINS · {1} ROUNDS · {2} GAMES', fameEmpty: 'NO ENTRIES YET',
    fameLines: 'RECENT WINNING SENTENCES', fameLoading: 'LOADING …', matchLines: 'WINNING SENTENCES OF THIS MATCH',
    pageOf: '{0}/{1}', previous: 'Previous page', nextPage: 'Next page',
    bigScreen: 'BIG SCREEN', bigScreenButton: '[ BIG SCREEN ]',
    bigScreenText: 'Open on a TV or laptop: shows prompt, clock and sentences for everyone.',
    copyScreenLink: '[ COPY LINK ]', openScreen: '[ OPEN ]', screenQrLabel: 'QR code for the big screen link',
    screenTitle: 'C/A/W · Big Screen {0}', screenJoin: 'SCAN WITH YOUR PHONE OR ENTER THE CODE',
    submittedBar: 'SUBMITTED', fullscreen: '[ FULL SCREEN ]',

    errors: {
      rejected: 'NOT POSSIBLE', not_host: 'HOST ONLY', already_joined: 'ALREADY JOINED',
      game_running: 'GAME ALREADY RUNNING · WAIT FOR THE NEXT LOBBY', session_full: 'SESSION FULL',
      locked: 'LOBBY LOCKED · ASK THE HOST', rate_limited: 'TOO MANY TRIES · WAIT A MOMENT',
    },
  },
};

let language = 'de';

export function currentLanguage() {
  return language;
}

export function setTextLanguage(code) {
  language = code === 'en' ? 'en' : 'de';
  document.documentElement.lang = language;
}

// t('round', 3, 8) -> 'RUNDE 3/8'
export function t(key, ...args) {
  const template = texts[language][key] ?? texts.de[key] ?? key;
  return template.replace(/\{(\d+)\}/g, (_, index) => String(args[Number(index)] ?? ''));
}

export function errorText(code) {
  return texts[language].errors[code] ?? texts[language].errors.rejected;
}

// Fills every element marked with data-text, data-label or data-placeholder.
export function applyStaticTexts(root = document) {
  for (const element of root.querySelectorAll('[data-text]')) element.textContent = t(element.dataset.text);
  for (const element of root.querySelectorAll('[data-label]')) element.setAttribute('aria-label', t(element.dataset.label));
  for (const element of root.querySelectorAll('[data-placeholder]')) element.placeholder = t(element.dataset.placeholder);
}
