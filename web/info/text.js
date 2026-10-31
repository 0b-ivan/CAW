// Texts of the info pages (licenses, privacy, imprint) in German and English.
// The privacy text describes what this software stores; the operator checks it before going public.
export const texts = {
  de: {
    back: '[ ZUR STARTSEITE ]',
    licensesTitle: 'LIZENZEN',
    privacyTitle: 'DATENSCHUTZ',
    imprintTitle: 'IMPRESSUM',
    packsHeading: 'KARTEN',
    packLine: '{0} – {1}, Rechteinhaber: {2}',
    packsNote: 'Die Karten stehen unter ihrer eigenen Lizenz, unabhängig vom Programmcode. ' +
      'CC BY-NC-SA 2.0 erlaubt keine kommerzielle Nutzung.',
    packsMissing: 'Die Liste der Packs ist gerade nicht erreichbar.',
    codeHeading: 'PROGRAMM',
    codeLines: [
      'Spielregeln: portiert aus der CrossPlay-X4-Firmware (MIT-Lizenz).',
      'uWebSockets und redis-plus-plus: Apache-2.0. nlohmann/json und doctest: MIT. ' +
        'libpqxx und hiredis: BSD. Die Lizenztexte liegen im Server-Image unter /usr/share/doc/caw.',
      'Die Lizenz des CAW-Anwendungscodes ist noch nicht festgelegt.',
    ],
    privacyParagraphs: [
      'Diese Seite braucht kein Konto und setzt keine Cookies. Es gibt kein Tracking und keine Werbung.',
      'Im Browser (localStorage) liegen: eine zufällige Geräte-ID für die Statistik, dein Sitz-Ticket und ' +
        'ggf. der Host-Schlüssel je Session sowie deine Sprachwahl. Du kannst sie über die ' +
        'Browser-Einstellungen löschen.',
      'Während einer Session speichert der Server den Spielstand mit deinem Namen in Redis. Er verfällt ' +
        '24 Stunden nach der letzten Aktivität.',
      'Für die Ruhmeshalle speichert der Server in Postgres: Geräte-ID, Name, Wort, Zahl der Spiele, Siege ' +
        'und Runden sowie Siegersätze. Spieler, die 180 Tage nicht gespielt haben, werden gelöscht; es bleiben ' +
        'höchstens die 500 neuesten Siegersätze. Die Geräte-ID wird nie an andere Geräte geschickt.',
      'IP-Adressen werden nicht gespeichert. Für die Begrenzung von Anfragen hält der Server sie kurz im ' +
        'Arbeitsspeicher.',
      'Verantwortlich ist der Betreiber, siehe Impressum.',
    ],
    imprintMissing: 'Die Betreiberangaben fehlen noch. Vor dem öffentlichen Betrieb web/operator.json ausfüllen.',
    imprintLines: ['Verantwortlich: {0}', 'Anschrift: {1}', 'E-Mail: {2}'],
  },
  en: {
    back: '[ TO THE START PAGE ]',
    licensesTitle: 'LICENSES',
    privacyTitle: 'PRIVACY',
    imprintTitle: 'IMPRINT',
    packsHeading: 'CARDS',
    packLine: '{0} – {1}, rights holder: {2}',
    packsNote: 'The cards have their own license, separate from the program code. ' +
      'CC BY-NC-SA 2.0 does not allow commercial use.',
    packsMissing: 'The list of packs cannot be reached right now.',
    codeHeading: 'PROGRAM',
    codeLines: [
      'Game rules: ported from the CrossPlay X4 firmware (MIT license).',
      'uWebSockets and redis-plus-plus: Apache-2.0. nlohmann/json and doctest: MIT. ' +
        'libpqxx and hiredis: BSD. The license texts are in the server image under /usr/share/doc/caw.',
      'The license of the CAW application code has not been chosen yet.',
    ],
    privacyParagraphs: [
      'This site needs no account and sets no cookies. There is no tracking and no advertising.',
      'Your browser (localStorage) keeps: a random device id for the statistics, your seat ticket and, if ' +
        'you host, the host key per session, and your language. You can delete them in your browser settings.',
      'During a session the server keeps the game state with your name in Redis. It expires 24 hours after ' +
        'the last activity.',
      'For the Wall of Fame the server stores in Postgres: device id, name, word, number of games, wins and ' +
        'rounds, and winning sentences. Players who have not played for 180 days are deleted; at most the 500 ' +
        'newest winning sentences are kept. The device id is never sent to other devices.',
      'IP addresses are not stored. To limit requests the server keeps them briefly in memory.',
      'The operator is responsible, see the imprint.',
    ],
    imprintMissing: 'The operator details are missing. Fill in web/operator.json before going public.',
    imprintLines: ['Responsible: {0}', 'Address: {1}', 'Email: {2}'],
  },
};
