/* Porpoise UI - the menus in the player's language.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * First translations, to be read over by native speakers: corrections go in
 * /data/porpoise/lang/es.txt (fr.txt, pt.txt, it.txt, ja.txt) as "English = translation"
 * lines and win over this table, so a fix needs no new build. */
#include "ui_i18n.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace porpoise::ui
{
namespace
{
struct Entry
{
    const char *en, *es, *fr, *pt, *it, *ja;
};

/* clang-format off */
const Entry kTable[] = {
    /* Top bar, tabs, library */
    {"Library", "Biblioteca", "Bibliothèque", "Biblioteca", "Libreria", "ライブラリー"},
    {"Memory Cards", "Tarjetas de memoria", "Cartes mémoire", "Cartões de memória", "Memory Card", "メモリーカード"},
    {"Settings", "Ajustes", "Paramètres", "Definições", "Impostazioni", "設定"},
    {"Your games", "Tus juegos", "Tes jeux", "Os teus jogos", "I tuoi giochi", "あなたのゲーム"},
    {"1 game", "1 juego", "1 jeu", "1 jogo", "1 gioco", "ゲーム1本"},
    {"{n} games", "{n} juegos", "{n} jeux", "{n} jogos", "{n} giochi", "ゲーム{n}本"},
    {"No games yet", "Aún no hay juegos", "Pas encore de jeux", "Ainda sem jogos", "Ancora nessun gioco", "ゲームはまだありません"},
    {"Add games", "Añade juegos", "Ajoute des jeux", "Adiciona jogos", "Aggiungi giochi", "ゲームを追加"},
    {"Copy .iso, .rvz or .ciso files with PS5 Upload into", "Copia archivos .iso, .rvz o .ciso con PS5 Upload en", "Copie des fichiers .iso, .rvz ou .ciso avec PS5 Upload dans", "Copia ficheiros .iso, .rvz ou .ciso com o PS5 Upload para", "Copia file .iso, .rvz o .ciso con PS5 Upload in", "PS5 Uploadで.iso、.rvz、.cisoファイルを次の場所にコピーしてください："},
    {"or add your own folder in Settings, under Games.", "o añade tu propia carpeta en Ajustes, en Juegos.", "ou ajoute ton propre dossier dans Paramètres, sous Jeux.", "ou adiciona a tua própria pasta em Definições, em Jogos.", "oppure aggiungi una tua cartella in Impostazioni, sotto Giochi.", "または、設定の「ゲーム」で自分のフォルダーを追加してください。"},
    {"Play", "Jugar", "Jouer", "Jogar", "Gioca", "プレイ"},
    {"Browse", "Explorar", "Parcourir", "Navegar", "Sfoglia", "見る"},
    {"Details", "Detalles", "Détails", "Detalhes", "Dettagli", "詳細"},
    {"Sort", "Ordenar", "Trier", "Ordenar", "Ordina", "並べ替え"},
    {"Sort games", "Ordenar juegos", "Trier les jeux", "Ordenar jogos", "Ordina giochi", "ゲームの並べ替え"},
    {"Title A-Z", "Título A-Z", "Titre A-Z", "Título A-Z", "Titolo A-Z", "タイトル順（A-Z）"},
    {"Recently played", "Jugados recientemente", "Joués récemment", "Jogados recentemente", "Giocati di recente", "最近プレイした順"},
    {"Current", "Actual", "Actuel", "Atual", "Attuale", "現在"},
    {"Back", "Atrás", "Retour", "Voltar", "Indietro", "戻る"},
    {"Choose", "Elegir", "Choisir", "Escolher", "Scegli", "選ぶ"},
    {"Confirm", "Confirmar", "Confirmer", "Confirmar", "Conferma", "決定"},
    {"Cancel", "Cancelar", "Annuler", "Cancelar", "Annulla", "キャンセル"},
    {"OK", "Aceptar", "OK", "OK", "OK", "OK"},
    {"Select", "Seleccionar", "Sélectionner", "Selecionar", "Seleziona", "選択"},

    /* Play history */
    {"Not played yet", "Aún sin jugar", "Pas encore joué", "Ainda não jogado", "Non ancora giocato", "未プレイ"},
    {"Played just now", "Jugado hace un momento", "Joué à l'instant", "Jogado agora mesmo", "Giocato poco fa", "たった今プレイ"},
    {"Played today", "Jugado hoy", "Joué aujourd'hui", "Jogado hoje", "Giocato oggi", "今日プレイ"},
    {"Last played yesterday", "Jugado por última vez ayer", "Joué pour la dernière fois hier", "Jogado pela última vez ontem", "Ultima partita ieri", "昨日プレイ"},
    {"Last played {n} days ago", "Jugado hace {n} días", "Joué il y a {n} jours", "Jogado há {n} dias", "Ultima partita {n} giorni fa", "{n}日前にプレイ"},
    {"Last played {n} weeks ago", "Jugado hace {n} semanas", "Joué il y a {n} semaines", "Jogado há {n} semanas", "Ultima partita {n} settimane fa", "{n}週間前にプレイ"},
    {"Last played a while ago", "Jugado hace tiempo", "Joué il y a longtemps", "Jogado há algum tempo", "Ultima partita tempo fa", "しばらく前にプレイ"},

    /* Regions */
    {"USA", "EE. UU.", "États-Unis", "EUA", "USA", "アメリカ"},
    {"Japan", "Japón", "Japon", "Japão", "Giappone", "日本"},
    {"Korea", "Corea", "Corée", "Coreia", "Corea", "韓国"},
    {"Europe", "Europa", "Europe", "Europa", "Europa", "ヨーロッパ"},
    {"Germany", "Alemania", "Allemagne", "Alemanha", "Germania", "ドイツ"},
    {"France", "Francia", "France", "França", "Francia", "フランス"},
    {"Spain", "España", "Espagne", "Espanha", "Spagna", "スペイン"},
    {"Italy", "Italia", "Italie", "Itália", "Italia", "イタリア"},
    {"Australia", "Australia", "Australie", "Austrália", "Australia", "オーストラリア"},

    /* Details */
    {"Developer", "Desarrollador", "Développeur", "Produtora", "Sviluppatore", "開発元"},
    {"Publisher", "Editor", "Éditeur", "Editora", "Editore", "発売元"},
    {"Released", "Lanzamiento", "Sortie", "Lançamento", "Uscita", "発売日"},
    {"Genre", "Género", "Genre", "Género", "Genere", "ジャンル"},
    {"Players", "Jugadores", "Joueurs", "Jogadores", "Giocatori", "プレイ人数"},
    {"Rating", "Clasificación", "Classification", "Classificação", "Classificazione", "レーティング"},
    {"Last played", "Última partida", "Dernière partie", "Última vez", "Ultima partita", "最終プレイ"},
    {"File", "Archivo", "Fichier", "Ficheiro", "File", "ファイル"},
    {"No description yet. It arrives with the game info from GameTDB.com.", "Aún no hay descripción. Llegará con la información del juego de GameTDB.com.", "Pas encore de description. Elle arrivera avec les infos du jeu de GameTDB.com.", "Ainda sem descrição. Chega com a informação do jogo do GameTDB.com.", "Nessuna descrizione per ora. Arriva con le informazioni del gioco da GameTDB.com.", "説明はまだありません。GameTDB.comのゲーム情報と一緒に届きます。"},
    {"Turn on Settings > Games > Download game info for a description.", "Activa Ajustes > Juegos > Descargar información para ver una descripción.", "Active Paramètres > Jeux > Télécharger les infos pour avoir une description.", "Ativa Definições > Jogos > Transferir informação para ver uma descrição.", "Attiva Impostazioni > Giochi > Scarica info giochi per avere una descrizione.", "説明を表示するには「設定 > ゲーム > ゲーム情報をダウンロード」をオンにしてください。"},
    {"Game settings", "Ajustes del juego", "Paramètres du jeu", "Definições do jogo", "Impostazioni del gioco", "ゲーム設定"},
    {"Back of box", "Reverso de la caja", "Dos de la boîte", "Verso da caixa", "Retro della confezione", "パッケージ裏面"},
    {"Front of box", "Frente de la caja", "Face de la boîte", "Frente da caixa", "Fronte della confezione", "パッケージ表面"},
    {"Right stick: turn the box", "Stick derecho: girar la caja", "Stick droit : tourner la boîte", "Analógico direito: rodar a caixa", "Levetta destra: ruota la confezione", "右スティック：パッケージを回す"},
    {"Save data", "Datos guardados", "Sauvegardes", "Dados guardados", "Dati di salvataggio", "セーブデータ"},
    {"Custom", "Personalizado", "Personnalisé", "Personalizado", "Personalizzato", "カスタム"},

    /* Memory cards */
    {"Slot {slot}", "Ranura {slot}", "Port {slot}", "Ranhura {slot}", "Slot {slot}", "スロット{slot}"},
    {"SLOT {slot}", "RANURA {slot}", "PORT {slot}", "RANHURA {slot}", "SLOT {slot}", "スロット{slot}"},
    {"memcard|Open", "Libres", "Libres", "Livres", "Liberi", "空き"},
    {"Open", "Abrir", "Ouvrir", "Abrir", "Apri", "開く"},
    {"No saves yet", "Sin partidas guardadas", "Pas de sauvegarde", "Sem jogos guardados", "Ancora nessun salvataggio", "セーブデータはまだありません"},
    {"1 save", "1 partida", "1 sauvegarde", "1 jogo guardado", "1 salvataggio", "セーブデータ1件"},
    {"{n} saves", "{n} partidas", "{n} sauvegardes", "{n} jogos guardados", "{n} salvataggi", "セーブデータ{n}件"},
    {"1 block", "1 bloque", "1 bloc", "1 bloco", "1 blocco", "1ブロック"},
    {"{n} blocks", "{n} bloques", "{n} blocs", "{n} blocos", "{n} blocchi", "{n}ブロック"},
    {"Nothing saved in Slot {slot} yet", "Aún no hay nada en la ranura {slot}", "Rien dans le port {slot} pour l'instant", "Ainda nada guardado na ranhura {slot}", "Ancora niente salvato nello Slot {slot}", "スロット{slot}にはまだ何もセーブされていません"},
    {"Your saves appear here after you save in a game.", "Tus partidas aparecen aquí cuando guardas en un juego.", "Tes sauvegardes apparaissent ici quand tu sauvegardes dans un jeu.", "Os teus jogos guardados aparecem aqui quando guardas num jogo.", "I tuoi salvataggi compaiono qui dopo che salvi in un gioco.", "ゲームでセーブすると、ここにセーブデータが表示されます。"},
    {"Copy to {slot}", "Copiar a {slot}", "Copier vers {slot}", "Copiar para {slot}", "Copia in {slot}", "{slot}にコピー"},
    {"Copy", "Copiar", "Copier", "Copiar", "Copia", "コピー"},
    {"Delete", "Borrar", "Supprimer", "Apagar", "Elimina", "削除"},
    {"Replace", "Reemplazar", "Remplacer", "Substituir", "Sostituisci", "上書き"},
    {"Delete this save?", "¿Borrar esta partida?", "Supprimer cette sauvegarde ?", "Apagar este jogo guardado?", "Eliminare questo salvataggio?", "このセーブデータを削除しますか？"},
    {"{save}. It is removed from Slot {slot} for good.", "{save}. Se borrará de la ranura {slot} para siempre.", "{save}. Elle sera supprimée du port {slot} pour de bon.", "{save}. É apagado da ranhura {slot} para sempre.", "{save}. Verrà rimosso dallo Slot {slot} per sempre.", "{save}。スロット{slot}から完全に削除されます。"},
    {"Copy to Slot {slot}?", "¿Copiar a la ranura {slot}?", "Copier vers le port {slot} ?", "Copiar para a ranhura {slot}?", "Copiare nello Slot {slot}?", "スロット{slot}にコピーしますか？"},
    {"{save} ({blocks}) is copied to Slot {slot}.", "{save} ({blocks}) se copiará a la ranura {slot}.", "{save} ({blocks}) sera copiée vers le port {slot}.", "{save} ({blocks}) é copiado para a ranhura {slot}.", "{save} ({blocks}) verrà copiato nello Slot {slot}.", "{save}（{blocks}）をスロット{slot}にコピーします。"},
    {"Replace the save on Slot {slot}?", "¿Reemplazar la partida de la ranura {slot}?", "Remplacer la sauvegarde du port {slot} ?", "Substituir o jogo guardado na ranhura {slot}?", "Sostituire il salvataggio nello Slot {slot}?", "スロット{slot}のセーブデータを上書きしますか？"},
    {"Slot {slot} already has {save}. Copying replaces it with this one.", "La ranura {slot} ya tiene {save}. Al copiar se reemplaza por esta.", "Le port {slot} contient déjà {save}. La copie la remplace par celle-ci.", "A ranhura {slot} já tem {save}. Copiar substitui-o por este.", "Lo Slot {slot} contiene già {save}. Copiando, verrà sostituito da questo.", "スロット{slot}にはすでに{save}があります。コピーするとこのセーブデータで上書きされます。"},
    {"Not enough room", "No hay espacio suficiente", "Pas assez de place", "Sem espaço suficiente", "Spazio insufficiente", "空き容量が足りません"},
    {"Slot {slot} has {free} blocks open; this save needs {need}.", "La ranura {slot} tiene {free} bloques libres; esta partida necesita {need}.", "Le port {slot} a {free} blocs libres ; cette sauvegarde en demande {need}.", "A ranhura {slot} tem {free} blocos livres; este jogo guardado precisa de {need}.", "Lo Slot {slot} ha {free} blocchi liberi; questo salvataggio ne richiede {need}.", "スロット{slot}の空きは{free}ブロックです。このセーブデータには{need}ブロック必要です。"},
    {"This save is inside a card file", "Esta partida está dentro de un archivo de tarjeta", "Cette sauvegarde est dans un fichier de carte", "Este jogo guardado está dentro de um ficheiro de cartão", "Questo salvataggio è dentro un file della Memory Card", "このセーブデータはカードファイルの中にあります"},
    {"Porpoise can delete saves that Dolphin keeps as files (its GCI folders), not ones inside a .raw memory card image.", "Porpoise puede borrar partidas que Dolphin guarda como archivos (sus carpetas GCI), no las de una imagen .raw de tarjeta.", "Porpoise peut supprimer les sauvegardes que Dolphin garde en fichiers (ses dossiers GCI), pas celles d'une image .raw.", "O Porpoise apaga jogos que o Dolphin guarda como ficheiros (pastas GCI), não os de uma imagem .raw de cartão.", "Porpoise può eliminare i salvataggi che Dolphin conserva come file (le sue cartelle GCI), non quelli dentro un'immagine .raw di Memory Card.", "Porpoiseで削除できるのは、Dolphinがファイルとして保存しているセーブデータ（GCIフォルダー）だけです。.rawメモリーカードイメージ内のセーブデータは削除できません。"},
    {"This save can't be copied", "Esta partida no se puede copiar", "Cette sauvegarde ne peut pas être copiée", "Este jogo guardado não pode ser copiado", "Questo salvataggio non si può copiare", "このセーブデータはコピーできません"},
    {"Porpoise copies saves that Dolphin keeps as files (its GCI folders).", "Porpoise copia partidas que Dolphin guarda como archivos (sus carpetas GCI).", "Porpoise copie les sauvegardes que Dolphin garde en fichiers (ses dossiers GCI).", "O Porpoise copia jogos que o Dolphin guarda como ficheiros (as pastas GCI).", "Porpoise copia i salvataggi che Dolphin conserva come file (le sue cartelle GCI).", "Porpoiseでコピーできるのは、Dolphinがファイルとして保存しているセーブデータ（GCIフォルダー）です。"},
    {"Could not delete the save", "No se pudo borrar la partida", "Impossible de supprimer la sauvegarde", "Não foi possível apagar o jogo guardado", "Impossibile eliminare il salvataggio", "セーブデータを削除できませんでした"},
    {"The file could not be removed: {path}", "No se pudo borrar el archivo: {path}", "Le fichier n'a pas pu être supprimé : {path}", "Não foi possível remover o ficheiro: {path}", "Impossibile rimuovere il file: {path}", "ファイルを削除できませんでした：{path}"},
    {"Could not copy the save", "No se pudo copiar la partida", "Impossible de copier la sauvegarde", "Não foi possível copiar o jogo guardado", "Impossibile copiare il salvataggio", "セーブデータをコピーできませんでした"},
    {"Writing {path} failed.", "Falló al escribir {path}.", "L'écriture de {path} a échoué.", "Falhou a escrita de {path}.", "Scrittura di {path} non riuscita.", "{path}への書き込みに失敗しました。"},

    /* Launch */
    {"Launching", "Iniciando", "Lancement", "A iniciar", "Avvio", "起動中"},
    {"Starting {game}\xE2\x80\xA6", "Iniciando {game}\xE2\x80\xA6", "Lancement de {game}\xE2\x80\xA6", "A iniciar {game}\xE2\x80\xA6", "Avvio di {game}…", "{game}を起動しています…"},
    {"Starting game\xE2\x80\xA6", "Iniciando juego\xE2\x80\xA6", "Lancement du jeu\xE2\x80\xA6", "A iniciar o jogo\xE2\x80\xA6", "Avvio del gioco…", "ゲームを起動しています…"},
    {"Loading Dolphin", "Cargando Dolphin", "Chargement de Dolphin", "A carregar o Dolphin", "Caricamento di Dolphin", "Dolphinを読み込み中"},
    {"Reading the disc", "Leyendo el disco", "Lecture du disque", "A ler o disco", "Lettura del disco", "ディスクを読み込み中"},
    {"Preparing graphics", "Preparando los gráficos", "Préparation des graphismes", "A preparar os gráficos", "Preparazione della grafica", "グラフィックスを準備中"},
    {"This game didn't start", "Este juego no arrancó", "Ce jeu n'a pas démarré", "Este jogo não arrancou", "Questo gioco non si è avviato", "ゲームを起動できませんでした"},
    {"Dolphin could not start it. The file may be damaged or in a format Porpoise can't read yet. Details are in porpoise/core.log.", "Dolphin no pudo iniciarlo. El archivo puede estar dañado o en un formato que Porpoise aún no lee. Detalles en porpoise/core.log.", "Dolphin n'a pas pu le lancer. Le fichier est peut-être abîmé ou dans un format que Porpoise ne lit pas encore. Détails dans porpoise/core.log.", "O Dolphin não o conseguiu iniciar. O ficheiro pode estar danificado ou num formato que o Porpoise ainda não lê. Detalhes em porpoise/core.log.", "Dolphin non è riuscito ad avviarlo. Il file potrebbe essere danneggiato o in un formato che Porpoise non legge ancora. Trovi i dettagli in porpoise/core.log.", "Dolphinでこのゲームを起動できませんでした。ファイルが破損しているか、Porpoiseがまだ読み込めない形式の可能性があります。詳しくはporpoise/core.logを確認してください。"},

    /* In-game menu */
    {"PAUSED", "EN PAUSA", "EN PAUSE", "EM PAUSA", "IN PAUSA", "一時停止中"},
    {"Resume", "Continuar", "Reprendre", "Continuar", "Riprendi", "再開"},
    {"FPS counter", "Contador de FPS", "Compteur FPS", "Contador de FPS", "Contatore FPS", "FPSカウンター"},
    {"Upscaling", "Escalado", "Mise à l'échelle", "Escala", "Upscaling", "アップスケール"},
    {"Volume", "Volumen", "Volume", "Volume", "Volume", "音量"},
    {"Quit to library", "Salir a la biblioteca", "Retour à la bibliothèque", "Sair para a biblioteca", "Torna alla libreria", "ライブラリーに戻る"},
    {"Close Porpoise", "Cerrar Porpoise", "Fermer Porpoise", "Fechar o Porpoise", "Chiudi Porpoise", "Porpoiseを終了"},
    {"Experimental: may slow some games down.", "Experimental: puede ralentizar algunos juegos.", "Expérimental : peut ralentir certains jeux.", "Experimental: pode tornar alguns jogos mais lentos.", "Sperimentale: può rallentare alcuni giochi.", "実験的機能：一部のゲームが遅くなることがあります。"},
    {"Changes here are saved for this game.", "Los cambios se guardan para este juego.", "Les changements sont gardés pour ce jeu.", "As alterações ficam guardadas para este jogo.", "Le modifiche qui vengono salvate per questo gioco.", "ここでの変更はこのゲームに保存されます。"},
    {"4x (1440p) \xE2\x80\xA2 exp.", "4x (1440p) \xE2\x80\xA2 exp.", "4x (1440p) \xE2\x80\xA2 exp.", "4x (1440p) \xE2\x80\xA2 exp.", "4x (1440p) • sper.", "4x (1440p) • 実験的"},

    /* Settings: sections */
    {"Games", "Juegos", "Jeux", "Jogos", "Giochi", "ゲーム"},
    {"Video", "Vídeo", "Vidéo", "Vídeo", "Video", "ビデオ"},
    {"Graphics", "Gráficos", "Graphismes", "Gráficos", "Grafica", "グラフィックス"},
    {"Audio", "Audio", "Audio", "Áudio", "Audio", "オーディオ"},
    {"Controls", "Controles", "Commandes", "Controlos", "Comandi", "操作"},
    {"System", "Sistema", "Système", "Sistema", "Sistema", "システム"},
    {"Interface", "Interfaz", "Interface", "Interface", "Interfaccia", "インターフェース"},
    {"About", "Acerca de", "À propos", "Sobre", "Info", "情報"},
    {"This game", "Este juego", "Ce jeu", "Este jogo", "Questo gioco", "このゲーム"},
    {"GAME SETTINGS", "AJUSTES DEL JUEGO", "PARAMÈTRES DU JEU", "DEFINIÇÕES DO JOGO", "IMPOSTAZIONI DEL GIOCO", "ゲーム設定"},
    {"Sections", "Secciones", "Sections", "Secções", "Sezioni", "セクション"},
    {"Change", "Cambiar", "Modifier", "Alterar", "Cambia", "変更"},
    {"Search", "Buscar", "Rechercher", "Procurar", "Cerca", "検索"},
    {"Choose a folder", "Elegir carpeta", "Choisir un dossier", "Escolher pasta", "Scegli una cartella", "フォルダーを選択"},
    {"Remove", "Quitar", "Retirer", "Remover", "Rimuovi", "外す"},
    {"Reset", "Restablecer", "Réinitialiser", "Repor", "Ripristina", "リセット"},
    {"Reset\xE2\x80\xA6", "Restablecer\xE2\x80\xA6", "Réinitialiser\xE2\x80\xA6", "Repor\xE2\x80\xA6", "Ripristina…", "リセット…"},
    {"Choose\xE2\x80\xA6", "Elegir\xE2\x80\xA6", "Choisir\xE2\x80\xA6", "Escolher\xE2\x80\xA6", "Scegli…", "選択…"},
    {"On", "Sí", "Oui", "Sim", "Sì", "オン"},
    {"Off", "No", "Non", "Não", "No", "オフ"},
    {"Changes apply the next time a game starts", "Los cambios se aplican la próxima vez que inicies un juego", "Les changements s'appliquent au prochain lancement d'un jeu", "As alterações aplicam-se na próxima vez que um jogo iniciar", "Le modifiche valgono dal prossimo avvio di un gioco", "次にゲームを起動したときに変更が適用されます"},
    {"Where Porpoise looks for games, and what it downloads for them", "Dónde busca Porpoise los juegos y qué descarga para ellos", "Où Porpoise cherche les jeux, et ce qu'il télécharge pour eux", "Onde o Porpoise procura jogos e o que transfere para eles", "Dove Porpoise cerca i giochi e cosa scarica per loro", "Porpoiseがゲームを探す場所と、ダウンロードするもの"},
    {"How Porpoise looks and reads", "Cómo se ve y se lee Porpoise", "L'apparence et la langue de Porpoise", "O aspeto e a língua do Porpoise", "Aspetto e leggibilità di Porpoise", "Porpoiseの見た目と文字"},
    {"Values in blue are this game's own", "Los valores en azul son propios de este juego", "Les valeurs en bleu sont propres à ce jeu", "Os valores a azul são deste jogo", "I valori in blu sono propri di questo gioco", "青い値はこのゲーム独自の設定です"},
    {"For this game only \xE2\x80\xA2 values in blue are its own", "Solo para este juego \xE2\x80\xA2 los valores en azul son suyos", "Pour ce jeu seulement \xE2\x80\xA2 les valeurs en bleu sont les siennes", "Só para este jogo \xE2\x80\xA2 os valores a azul são dele", "Solo per questo gioco • i valori in blu sono suoi", "このゲームのみ • 青い値はこのゲーム独自の設定です"},
    {"Choose a section with up and down, then press Right or Cross to go into it.", "Elige una sección con arriba y abajo, y pulsa Derecha o X para entrar.", "Choisis une section avec haut et bas, puis appuie sur Droite ou Croix pour y entrer.", "Escolhe uma secção com cima e baixo e prime Direita ou X para entrar.", "Scegli una sezione con su e giù, poi premi destra o Croce per entrarci.", "上下でセクションを選び、右か×で開きます。"},

    /* Settings: Games */
    {"Find games automatically", "Buscar juegos automáticamente", "Trouver les jeux automatiquement", "Encontrar jogos automaticamente", "Trova giochi automaticamente", "ゲームを自動で探す"},
    {"Looks in /data/porpoise/games, /data/games, /data/roms, /data/iso and on USB drives.", "Busca en /data/porpoise/games, /data/games, /data/roms, /data/iso y en unidades USB.", "Cherche dans /data/porpoise/games, /data/games, /data/roms, /data/iso et sur les clés USB.", "Procura em /data/porpoise/games, /data/games, /data/roms, /data/iso e em unidades USB.", "Cerca in /data/porpoise/games, /data/games, /data/roms, /data/iso e sulle unità USB.", "/data/porpoise/games、/data/games、/data/roms、/data/iso、USBドライブを探します。"},
    {"Download covers", "Descargar portadas", "Télécharger les jaquettes", "Transferir capas", "Scarica copertine", "パッケージ画像をダウンロード"},
    {"Box art from GameTDB.com, saved in /data/porpoise/covers. Needs the console online.", "Portadas de GameTDB.com, guardadas en /data/porpoise/covers. La consola debe estar en línea.", "Jaquettes de GameTDB.com, gardées dans /data/porpoise/covers. La console doit être en ligne.", "Capas do GameTDB.com, guardadas em /data/porpoise/covers. A consola tem de estar online.", "Copertine da GameTDB.com, salvate in /data/porpoise/covers. La console deve essere online.", "GameTDB.comのパッケージ画像を/data/porpoise/coversに保存します。本体のネットワーク接続が必要です。"},
    {"Download game info", "Descargar información de juegos", "Télécharger les infos des jeux", "Transferir informação dos jogos", "Scarica info giochi", "ゲーム情報をダウンロード"},
    {"Descriptions, developers, release dates and disc art from GameTDB.com, for Details.", "Descripciones, desarrolladores, fechas y arte del disco de GameTDB.com, para Detalles.", "Descriptions, développeurs, dates de sortie et images du disque de GameTDB.com, pour Détails.", "Descrições, produtoras, datas e arte do disco do GameTDB.com, para Detalhes.", "Descrizioni, sviluppatori, date di uscita e immagini dei dischi da GameTDB.com, per Dettagli.", "GameTDB.comから説明、開発元、発売日、ディスク画像を取得し、「詳細」に表示します。"},
    {"Porpoise looks in this folder and four levels below it. Cross stops looking here; files stay.", "Porpoise busca en esta carpeta y cuatro niveles más abajo. X deja de buscar aquí; los archivos se quedan.", "Porpoise cherche dans ce dossier et quatre niveaux en dessous. Croix arrête de chercher ici ; les fichiers restent.", "O Porpoise procura nesta pasta e em quatro níveis abaixo. X deixa de procurar aqui; os ficheiros ficam.", "Porpoise cerca in questa cartella e fino a quattro livelli sotto. Croce smette di cercare qui; i file restano.", "Porpoiseはこのフォルダーと、その4階層下まで探します。×でここを探すのをやめます（ファイルは残ります）。"},
    {"Add a game folder", "Añadir carpeta de juegos", "Ajouter un dossier de jeux", "Adicionar pasta de jogos", "Aggiungi cartella giochi", "ゲームフォルダーを追加"},
    {"Pick any folder on the console or a USB drive to search for games.", "Elige cualquier carpeta de la consola o de un USB para buscar juegos.", "Choisis n'importe quel dossier de la console ou d'une clé USB pour y chercher des jeux.", "Escolhe qualquer pasta da consola ou de uma unidade USB para procurar jogos.", "Scegli una cartella qualsiasi sulla console o su un'unità USB in cui cercare giochi.", "本体やUSBドライブの好きなフォルダーを選んで、ゲームを探せます。"},
    {"Search for games now", "Buscar juegos ahora", "Chercher les jeux maintenant", "Procurar jogos agora", "Cerca giochi ora", "今すぐゲームを探す"},
    {"Looks through every folder again, for games you have just copied over.", "Vuelve a revisar todas las carpetas, por si acabas de copiar juegos.", "Repasse tous les dossiers, pour les jeux que tu viens de copier.", "Volta a ver todas as pastas, para jogos que acabaste de copiar.", "Ricontrolla tutte le cartelle, per i giochi che hai appena copiato.", "すべてのフォルダーを探し直して、コピーしたばかりのゲームを見つけます。"},

    /* Settings: Video */
    {"Internal resolution", "Resolución interna", "Résolution interne", "Resolução interna", "Risoluzione interna", "内部解像度"},
    {"How sharp games render. 1080p is the tested default; above it is experimental and can slow games.", "Qué tan nítidos se ven los juegos. 1080p es lo probado; por encima es experimental y puede ralentizar.", "La netteté des jeux. 1080p est la valeur testée ; au-delà c'est expérimental et ça peut ralentir.", "A nitidez dos jogos. 1080p é o valor testado; acima disso é experimental e pode tornar lento.", "Quanto sono nitidi i giochi. 1080p è il valore predefinito testato; oltre è sperimentale e può rallentare i giochi.", "ゲームの描画の精細さです。1080pが検証済みの標準で、それ以上は実験的なため、ゲームが遅くなることがあります。"},
    {"4x (1440p) \xE2\x80\xA2 experimental", "4x (1440p) \xE2\x80\xA2 experimental", "4x (1440p) \xE2\x80\xA2 expérimental", "4x (1440p) \xE2\x80\xA2 experimental", "4x (1440p) • sperimentale", "4x (1440p) • 実験的"},
    {"5x (1800p) \xE2\x80\xA2 experimental", "5x (1800p) \xE2\x80\xA2 experimental", "5x (1800p) \xE2\x80\xA2 expérimental", "5x (1800p) \xE2\x80\xA2 experimental", "5x (1800p) • sperimentale", "5x (1800p) • 実験的"},
    {"6x (4K) \xE2\x80\xA2 experimental", "6x (4K) \xE2\x80\xA2 experimental", "6x (4K) \xE2\x80\xA2 expérimental", "6x (4K) \xE2\x80\xA2 experimental", "6x (4K) • sperimentale", "6x (4K) • 実験的"},
    {"Widescreen hack", "Hack de pantalla ancha", "Hack écran large", "Hack de ecrã panorâmico", "Hack widescreen", "ワイドスクリーンハック"},
    {"Draws games in 16:9. Some games show glitches at the screen edges.", "Muestra los juegos en 16:9. Algunos tienen fallos en los bordes.", "Affiche les jeux en 16:9. Certains montrent des défauts sur les bords.", "Mostra os jogos em 16:9. Alguns têm falhas nas margens.", "Mostra i giochi in 16:9. Alcuni giochi hanno difetti grafici ai bordi dello schermo.", "ゲームを16:9で描画します。画面の端に表示の乱れが出るゲームもあります。"},
    {"Aspect ratio", "Relación de aspecto", "Format d'image", "Proporção", "Proporzioni", "アスペクト比"},
    {"The picture's shape. Auto follows the game; Stretch fills the screen.", "La forma de la imagen. Auto sigue al juego; Estirar llena la pantalla.", "La forme de l'image. Auto suit le jeu ; Étirer remplit l'écran.", "A forma da imagem. Auto segue o jogo; Esticar preenche o ecrã.", "La forma dell'immagine. Auto segue il gioco; Adatta riempie lo schermo.", "画面の形です。「自動」はゲームに合わせ、「引き伸ばし」は画面いっぱいに表示します。"},
    {"Auto", "Auto", "Auto", "Auto", "Auto", "自動"},
    {"Force 16:9", "Forzar 16:9", "Forcer 16:9", "Forçar 16:9", "Forza 16:9", "16:9に固定"},
    {"Force 4:3", "Forzar 4:3", "Forcer 4:3", "Forçar 4:3", "Forza 4:3", "4:3に固定"},
    {"Stretch to fill", "Estirar", "Étirer", "Esticar", "Adatta allo schermo", "引き伸ばし"},
    {"Anti-aliasing", "Antialiasing", "Anticrénelage", "Anti-aliasing", "Anti-aliasing", "アンチエイリアス"},
    {"Smooths jagged edges. SSAA is the sharpest and the heaviest.", "Suaviza los bordes dentados. SSAA es el más nítido y el más pesado.", "Adoucit les bords crénelés. Le SSAA est le plus net et le plus lourd.", "Suaviza os contornos serrilhados. O SSAA é o mais nítido e o mais pesado.", "Ammorbidisce i bordi seghettati. SSAA è il più nitido e il più pesante.", "ギザギザの輪郭をなめらかにします。SSAAが最も鮮明ですが、最も重くなります。"},
    {"Anisotropic filtering", "Filtrado anisotrópico", "Filtrage anisotrope", "Filtragem anisotrópica", "Filtro anisotropico", "異方性フィルタリング"},
    {"Sharper textures on floors and walls seen at an angle.", "Texturas más nítidas en suelos y paredes vistos en ángulo.", "Textures plus nettes sur les sols et les murs vus de biais.", "Texturas mais nítidas em chãos e paredes vistos de lado.", "Texture più nitide su pavimenti e muri visti di sbieco.", "斜めから見た床や壁のテクスチャーをくっきりさせます。"},
    {"Texture filtering", "Filtrado de texturas", "Filtrage des textures", "Filtragem de texturas", "Filtro texture", "テクスチャーフィルタリング"},
    {"Force sharp or smooth textures, or leave it to the game.", "Fuerza texturas nítidas o suaves, o deja que decida el juego.", "Force des textures nettes ou lisses, ou laisse le jeu décider.", "Força texturas nítidas ou suaves, ou deixa o jogo decidir.", "Forza texture nitide o morbide, oppure lascia decidere al gioco.", "テクスチャーをくっきり、またはなめらかに固定するか、ゲームに任せます。"},
    {"Game's own", "El del juego", "Celui du jeu", "O do jogo", "Del gioco", "ゲーム標準"},
    {"Nearest (sharp)", "Vecino (nítido)", "Plus proche (net)", "Vizinho (nítido)", "Nearest (nitido)", "ニアレスト（くっきり）"},
    {"Linear (smooth)", "Lineal (suave)", "Linéaire (lisse)", "Linear (suave)", "Lineare (morbido)", "リニア（なめらか）"},
    {"Output resampling", "Remuestreo de salida", "Rééchantillonnage", "Reamostragem de saída", "Ricampionamento in uscita", "出力リサンプリング"},
    {"How Dolphin scales its picture. Sharp bilinear keeps pixels crisp.", "Cómo escala Dolphin la imagen. Bilineal nítido mantiene los píxeles definidos.", "Comment Dolphin met l'image à l'échelle. Bilinéaire net garde les pixels précis.", "Como o Dolphin escala a imagem. Bilinear nítido mantém os píxeis definidos.", "Come Dolphin ridimensiona l'immagine. Bilineare nitido mantiene i pixel definiti.", "Dolphinの映像の拡大方法です。「シャープバイリニア」はピクセルをくっきり保ちます。"},
    {"Default", "Predeterminado", "Par défaut", "Predefinido", "Predefinito", "標準"},
    {"Sharp bilinear", "Bilineal nítido", "Bilinéaire net", "Bilinear nítido", "Bilineare nitido", "シャープバイリニア"},
    {"Area sampling", "Muestreo por área", "Échantillonnage par zone", "Amostragem por área", "Campionamento ad area", "エリアサンプリング"},
    {"Upscaling to the TV", "Escalado a la TV", "Mise à l'échelle sur la TV", "Escala para a TV", "Upscaling verso la TV", "テレビへのアップスケール"},
    {"How Porpoise fits the picture to your TV.", "Cómo ajusta Porpoise la imagen a tu TV.", "Comment Porpoise adapte l'image à ta TV.", "Como o Porpoise ajusta a imagem à tua TV.", "Come Porpoise adatta l'immagine alla tua TV.", "Porpoiseが映像をテレビに合わせる方法です。"},
    {"Smooth", "Suave", "Lisse", "Suave", "Morbido", "なめらか"},
    {"Sharp", "Nítido", "Net", "Nítido", "Nitido", "くっきり"},
    {"FPS overlay", "Mostrar FPS", "Afficher les FPS", "Mostrar FPS", "Mostra FPS", "FPS表示"},
    {"Shows the frame rate in the corner while you play.", "Muestra los fotogramas por segundo en una esquina mientras juegas.", "Affiche les images par seconde dans un coin pendant que tu joues.", "Mostra as imagens por segundo num canto enquanto jogas.", "Mostra il frame rate in un angolo mentre giochi.", "プレイ中、画面の隅にフレームレートを表示します。"},

    /* Settings: Graphics */
    {"Shader compilation", "Compilación de shaders", "Compilation des shaders", "Compilação de shaders", "Compilazione shader", "シェーダーコンパイル"},
    {"Ubershaders hide the stutter when a game draws something new, at a GPU cost.", "Los ubershaders evitan tirones cuando el juego dibuja algo nuevo, a costa de la GPU.", "Les ubershaders évitent les saccades quand le jeu affiche du nouveau, au prix du GPU.", "Os ubershaders evitam soluços quando o jogo desenha algo novo, à custa da GPU.", "Gli ubershader nascondono gli scatti quando un gioco disegna qualcosa di nuovo, a spese della GPU.", "Ubershadersを使うと、ゲームが新しいものを描画するときのカクつきを抑えられますが、GPUの負荷が増えます。"},
    {"Synchronous", "Síncrono", "Synchrone", "Síncrono", "Sincrona", "同期"},
    {"Ubershaders", "Ubershaders", "Ubershaders", "Ubershaders", "Ubershader", "Ubershaders"},
    {"Async ubershaders", "Ubershaders asíncronos", "Ubershaders asynchrones", "Ubershaders assíncronos", "Ubershader asincroni", "非同期（Ubershaders）"},
    {"Async, skip drawing", "Asíncrono, sin dibujar", "Asynchrone, sans dessin", "Assíncrono, sem desenhar", "Asincrona, salta il disegno", "非同期（描画をスキップ）"},
    {"Texture cache accuracy", "Precisión de la caché de texturas", "Précision du cache de textures", "Precisão da cache de texturas", "Precisione cache texture", "テクスチャーキャッシュの精度"},
    {"Safe fixes some games' text and effects; Fast is quickest.", "Seguro arregla texto y efectos de algunos juegos; Rápido es lo más veloz.", "Sûr corrige le texte et les effets de certains jeux ; Rapide est le plus vif.", "Seguro corrige texto e efeitos de alguns jogos; Rápido é o mais veloz.", "Sicura corregge testi ed effetti di alcuni giochi; Veloce è la più rapida.", "「安全」は一部のゲームの文字や効果を修正します。「高速」が最も速くなります。"},
    {"Fast", "Rápido", "Rapide", "Rápido", "Veloce", "高速"},
    {"Middle", "Medio", "Moyen", "Médio", "Media", "中"},
    {"Safe", "Seguro", "Sûr", "Seguro", "Sicura", "安全"},
    {"Per-pixel lighting", "Iluminación por píxel", "Éclairage par pixel", "Iluminação por píxel", "Illuminazione per pixel", "ピクセル単位ライティング"},
    {"Smoother lighting on surfaces. A little heavier.", "Iluminación más suave en superficies. Algo más pesada.", "Un éclairage plus doux sur les surfaces. Un peu plus lourd.", "Iluminação mais suave nas superfícies. Um pouco mais pesada.", "Illuminazione più morbida sulle superfici. Un po' più pesante.", "表面の光がよりなめらかになります。少し重くなります。"},
    {"Disable fog", "Desactivar niebla", "Désactiver le brouillard", "Desativar nevoeiro", "Disattiva nebbia", "フォグを無効化"},
    {"Removes distance fog. Some games use fog for their look.", "Quita la niebla lejana. Algunos juegos la usan para su estilo.", "Retire le brouillard au loin. Certains jeux l'utilisent pour leur ambiance.", "Remove o nevoeiro ao longe. Alguns jogos usam-no para o seu visual.", "Rimuove la nebbia in lontananza. Alcuni giochi la usano per il loro stile.", "遠くのフォグを消します。フォグを演出に使っているゲームもあります。"},
    {"Crop overscan", "Recortar bordes", "Rogner les bords", "Cortar margens", "Ritaglia overscan", "オーバースキャンをカット"},
    {"Hides the black borders some games draw at the edges.", "Oculta los bordes negros que dibujan algunos juegos.", "Masque les bandes noires que certains jeux dessinent sur les bords.", "Esconde as margens pretas que alguns jogos desenham.", "Nasconde i bordi neri che alcuni giochi disegnano ai lati.", "一部のゲームが画面の端に描く黒い帯を隠します。"},
    {"Custom textures", "Texturas personalizadas", "Textures personnalisées", "Texturas personalizadas", "Texture personalizzate", "カスタムテクスチャー"},
    {"Loads texture packs from /data/porpoise/saves/User/Load/Textures/<game ID>.", "Carga packs de texturas de /data/porpoise/saves/User/Load/Textures/<ID del juego>.", "Charge les packs de textures depuis /data/porpoise/saves/User/Load/Textures/<ID du jeu>.", "Carrega packs de texturas de /data/porpoise/saves/User/Load/Textures/<ID do jogo>.", "Carica i pacchetti di texture da /data/porpoise/saves/User/Load/Textures/<ID del gioco>.", "/data/porpoise/saves/User/Load/Textures/<ゲームID>からテクスチャーパックを読み込みます。"},
    {"Skip duplicate frames", "Omitir fotogramas duplicados", "Ignorer les images en double", "Saltar imagens repetidas", "Salta fotogrammi duplicati", "重複フレームをスキップ"},
    {"Saves work when a game shows the same frame twice.", "Ahorra trabajo cuando un juego muestra el mismo fotograma dos veces.", "Évite du travail quand un jeu affiche deux fois la même image.", "Poupa trabalho quando um jogo mostra a mesma imagem duas vezes.", "Risparmia lavoro quando un gioco mostra due volte lo stesso fotogramma.", "ゲームが同じフレームを2回表示するとき、処理を省きます。"},

    /* Settings: Audio, Controls */
    {"Menu music", "Música del menú", "Musique du menu", "Música do menu", "Musica del menu", "メニューBGM"},
    {"The music that plays in Porpoise's menus.", "La música que suena en los menús de Porpoise.", "La musique qui joue dans les menus de Porpoise.", "A música que toca nos menus do Porpoise.", "La musica che suona nei menu di Porpoise.", "Porpoiseのメニューで流れる音楽です。"},
    {"Music volume", "Volumen de la música", "Volume de la musique", "Volume da música", "Volume musica", "BGM音量"},
    {"How loud the menu music plays.", "Qué tan fuerte suena la música del menú.", "Le volume de la musique du menu.", "O volume da música do menu.", "Il volume della musica dei menu.", "メニューBGMの音量です。"},
    {"Menu sounds", "Sonidos del menú", "Sons du menu", "Sons do menu", "Suoni del menu", "メニューの効果音"},
    {"The sounds of moving through the menus.", "Los sonidos al moverte por los menús.", "Les sons quand tu te déplaces dans les menus.", "Os sons ao navegar pelos menus.", "I suoni quando ti muovi nei menu.", "メニューを移動するときの音です。"},
    {"Sounds volume", "Volumen de los sonidos", "Volume des sons", "Volume dos sons", "Volume suoni", "効果音の音量"},
    {"How loud the menu sounds play.", "Qué tan fuerte suenan los sonidos del menú.", "Le volume des sons du menu.", "O volume dos sons do menu.", "Il volume dei suoni dei menu.", "メニューの効果音の音量です。"},
    {"Game volume", "Volumen del juego", "Volume du jeu", "Volume do jogo", "Volume del gioco", "ゲームの音量"},
    {"Volume of the game's sound.", "Volumen del sonido del juego.", "Le volume du son du jeu.", "O volume do som do jogo.", "Il volume dell'audio del gioco.", "ゲームの音の音量です。"},
    {"Mute game", "Silenciar juego", "Couper le son du jeu", "Silenciar jogo", "Disattiva audio gioco", "ゲームをミュート"},
    {"Silences the game.", "Silencia el juego.", "Coupe le son du jeu.", "Silencia o jogo.", "Silenzia il gioco.", "ゲームの音を消します。"},
    {"Button layout", "Distribución de botones", "Disposition des boutons", "Disposição dos botões", "Configurazione tasti", "ボタン配置"},
    {"GameCube: Cross is A, like confirming on PlayStation. PlayStation: by position.", "GameCube: X es A, como confirmar en PlayStation. PlayStation: por posición.", "GameCube : Croix est A, comme pour valider sur PlayStation. PlayStation : par position.", "GameCube: X é A, como confirmar na PlayStation. PlayStation: pela posição.", "GameCube: Croce è A, come per confermare su PlayStation. PlayStation: in base alla posizione.", "GameCube：PlayStationの決定と同じく×がAです。PlayStation：ボタンの位置で割り当てます。"},
    {"PlayStation", "PlayStation", "PlayStation", "PlayStation", "PlayStation", "PlayStation"},
    {"GameCube", "GameCube", "GameCube", "GameCube", "GameCube", "GameCube"},
    {"Vibration", "Vibración", "Vibration", "Vibração", "Vibrazione", "振動"},
    {"Controller rumble.", "Vibración del mando.", "Les vibrations de la manette.", "A vibração do comando.", "Vibrazione del controller.", "コントローラーの振動です。"},
    {"GameCube: Cross is A, Square is B. PlayStation: Cross is A, Circle is B. Custom: your own buttons.", "GameCube: X es A y cuadrado es B. PlayStation: X es A y círculo es B. Personalizado: tus propios botones.", "GameCube : Croix est A, Carré est B. PlayStation : Croix est A, Rond est B. Personnalisé : tes propres boutons.", "GameCube: X é A e quadrado é B. PlayStation: X é A e círculo é B. Personalizado: os teus próprios botões.", "GameCube: Croce è A, Quadrato è B. PlayStation: Croce è A, Cerchio è B. Personalizzata: i tuoi tasti.", "GameCube：×がA、□がBです。PlayStation：×がA、○がBです。カスタム：自分で決めたボタンです。"},
    {"Customize buttons", "Personalizar botones", "Personnaliser les boutons", "Personalizar botões", "Personalizza tasti", "ボタンをカスタマイズ"},
    {"Give every GameCube button the DualSense button you like, on a picture of the controller.", "Asigna a cada botón de GameCube el botón del DualSense que quieras, sobre un dibujo del mando.", "Donne à chaque bouton GameCube le bouton DualSense de ton choix, sur un dessin de la manette.", "Dá a cada botão da GameCube o botão do DualSense que quiseres, num desenho do comando.", "Assegna a ogni tasto GameCube il tasto DualSense che preferisci, su un'immagine del controller.", "コントローラーの画像を見ながら、GameCubeの各ボタンにDualSenseの好きなボタンを割り当てます。"},
    {"Edit\xE2\x80\xA6", "Editar\xE2\x80\xA6", "Modifier\xE2\x80\xA6", "Editar\xE2\x80\xA6", "Modifica…", "編集…"},
    {"Customize", "Personalizar", "Personnaliser", "Personalizar", "Personalizza", "カスタマイズ"},
    {"Controllers", "Mandos", "Manettes", "Comandos", "Controller", "コントローラー"},
    {"Up to four players. To join, turn on another controller and choose a user for it.", "Hasta cuatro jugadores. Para unirse, enciende otro mando y elige un usuario para él.", "Jusqu'à quatre joueurs. Pour rejoindre, allume une autre manette et choisis-lui un utilisateur.", "Até quatro jogadores. Para entrar, liga outro comando e escolhe um utilizador para ele.", "Fino a quattro giocatori. Per unirti, accendi un altro controller e scegli un utente.", "最大4人までプレイできます。参加するには、別のコントローラーの電源を入れてユーザーを選んでください。"},
    {"1 player", "1 jugador", "1 joueur", "1 jogador", "1 giocatore", "1人"},
    {"{n} players", "{n} jugadores", "{n} joueurs", "{n} jogadores", "{n} giocatori", "{n}人"},
    {"Buttons", "Botones", "Boutons", "Botões", "Tasti", "ボタン"},
    {"For {game} only", "Solo para {game}", "Pour {game} seulement", "Só para {game}", "Solo per {game}", "{game}のみ"},
    {"Layout: {layout}", "Distribución: {layout}", "Disposition : {layout}", "Disposição: {layout}", "Configurazione: {layout}", "配置：{layout}"},
    {"Left stick: control stick   \xE2\x80\xA2   Right stick: C-stick", "Stick izquierdo: stick de control   \xE2\x80\xA2   Stick derecho: stick C", "Stick gauche : stick de contrôle   \xE2\x80\xA2   Stick droit : stick C", "Stick esquerdo: stick de controlo   \xE2\x80\xA2   Stick direito: stick C", "Levetta sinistra: stick di controllo   •   Levetta destra: C-stick", "左スティック：コントロールスティック   •   右スティック：Cスティック"},
    {"A button", "Botón A", "Bouton A", "Botão A", "Tasto A", "Aボタン"},
    {"B button", "Botón B", "Bouton B", "Botão B", "Tasto B", "Bボタン"},
    {"X button", "Botón X", "Bouton X", "Botão X", "Tasto X", "Xボタン"},
    {"Y button", "Botón Y", "Bouton Y", "Botão Y", "Tasto Y", "Yボタン"},
    {"Z button", "Botón Z", "Bouton Z", "Botão Z", "Tasto Z", "Zボタン"},
    {"L trigger", "Gatillo L", "Gâchette L", "Gatilho L", "Grilletto L", "Lトリガー"},
    {"R trigger", "Gatillo R", "Gâchette R", "Gatilho R", "Grilletto R", "Rトリガー"},
    {"Start", "Start", "Start", "Start", "Start", "スタート"},
    {"D-pad up", "Cruceta arriba", "Croix haut", "Direcional cima", "Croce direzionale su", "十字ボタン上"},
    {"D-pad down", "Cruceta abajo", "Croix bas", "Direcional baixo", "Croce direzionale giù", "十字ボタン下"},
    {"D-pad left", "Cruceta izquierda", "Croix gauche", "Direcional esquerda", "Croce direzionale sinistra", "十字ボタン左"},
    {"D-pad right", "Cruceta derecha", "Croix droite", "Direcional direita", "Croce direzionale destra", "十字ボタン右"},
    {"OPTIONS", "OPTIONS", "OPTIONS", "OPTIONS", "OPTIONS", "OPTIONS"},
    {"TOUCH PAD", "PANEL TÁCTIL", "PAVÉ TACTILE", "PAINEL TÁTIL", "TOUCHPAD", "タッチパッド"},
    {"Cross", "X", "Croix", "X", "Croce", "×ボタン"},
    {"Circle", "Círculo", "Rond", "Círculo", "Cerchio", "○ボタン"},
    {"Square", "Cuadrado", "Carré", "Quadrado", "Quadrato", "□ボタン"},
    {"Triangle", "Triángulo", "Triangle", "Triângulo", "Triangolo", "△ボタン"},
    {"Press a button\xE2\x80\xA6", "Pulsa un botón\xE2\x80\xA6", "Appuie sur un bouton\xE2\x80\xA6", "Carrega num botão\xE2\x80\xA6", "Premi un tasto…", "ボタンを押してください…"},
    {"Use the GameCube layout", "Usar la distribución GameCube", "Utiliser la disposition GameCube", "Usar a disposição GameCube", "Usa la configurazione GameCube", "GameCube配置を使う"},
    {"Use the PlayStation layout", "Usar la distribución PlayStation", "Utiliser la disposition PlayStation", "Usar a disposição PlayStation", "Usa la configurazione PlayStation", "PlayStation配置を使う"},
    {"In use", "En uso", "Utilisée", "Em uso", "In uso", "使用中"},
    {"Cross A \xE2\x80\xA2 Square B", "X A \xE2\x80\xA2 Cuadrado B", "Croix A \xE2\x80\xA2 Carré B", "X A \xE2\x80\xA2 Quadrado B", "Croce A • Quadrato B", "×がA • □がB"},
    {"Cross A \xE2\x80\xA2 Circle B", "X A \xE2\x80\xA2 Círculo B", "Croix A \xE2\x80\xA2 Rond B", "X A \xE2\x80\xA2 Círculo B", "Croce A • Cerchio B", "×がA • ○がB"},
    {"The {layout} layout is on.", "La distribución {layout} está activa.", "La disposition {layout} est active.", "A disposição {layout} está ativa.", "La configurazione {layout} è attiva.", "配置を{layout}に切り替えました。"},
    {"{button} is now on {control}.", "{button} ahora está en {control}.", "{button} est maintenant sur {control}.", "{button} está agora em {control}.", "{button} ora è su {control}.", "{control}に{button}を割り当てました。"},
    {"Nothing pressed; the button is unchanged.", "No se pulsó nada; el botón no cambia.", "Rien n'a été pressé ; le bouton ne change pas.", "Nada foi carregado; o botão fica igual.", "Nessun tasto premuto; il tasto resta invariato.", "何も押されなかったため、ボタンは変更されていません。"},
    {"Press the DualSense button for {button} ({n})", "Pulsa el botón del DualSense para {button} ({n})", "Appuie sur le bouton DualSense pour {button} ({n})", "Carrega no botão do DualSense para {button} ({n})", "Premi il tasto DualSense per {button} ({n})", "{button}に使うDualSenseのボタンを押してください（{n}）"},
    {"Use", "Usar", "Utiliser", "Usar", "Usa", "使う"},
    {"Other games", "Otros juegos", "Autres jeux", "Outros jogos", "Altri giochi", "ほかのゲーム"},
    {"A GameCube and Wii emulator for PS5, powered by Dolphin.", "Un emulador de GameCube y Wii para PS5, con Dolphin.", "Un émulateur GameCube et Wii pour PS5, propulsé par Dolphin.", "Um emulador de GameCube e Wii para a PS5, com o Dolphin.", "Un emulatore GameCube e Wii per PS5, basato su Dolphin.", "Dolphinを使った、PS5用のGameCube・Wiiエミュレーターです。"},
    {"Report a bug", "Informar de un error", "Signaler un bug", "Reportar um erro", "Segnala un bug", "不具合を報告"},
    {"Found a problem? Open an issue there with the game, what happened and porpoise/core.log.", "¿Algo falla? Abre un issue ahí con el juego, lo que pasó y porpoise/core.log.", "Un problème ? Ouvre un ticket là-bas avec le jeu, ce qui s'est passé et porpoise/core.log.", "Algo correu mal? Abre um issue lá com o jogo, o que aconteceu e o porpoise/core.log.", "Hai trovato un problema? Apri lì una segnalazione con il gioco, cosa è successo e porpoise/core.log.", "問題がありましたか？そこでIssueを作成し、ゲーム名、起きたこと、porpoise/core.logを添えてください。"},

    /* Settings: System */
    {"CPU clock", "Reloj de CPU", "Fréquence du CPU", "Relógio do CPU", "Clock CPU", "CPUクロック"},
    {"Overclocking can smooth a game that slows down. 100% is the real console.", "Subirlo puede suavizar un juego que se ralentiza. 100% es la consola real.", "L'augmenter peut fluidifier un jeu qui ralentit. 100 % est la vraie console.", "Aumentá-lo pode suavizar um jogo que fica lento. 100% é a consola real.", "L'overclock può rendere più fluido un gioco che rallenta. 100% è la console originale.", "オーバークロックすると、処理落ちするゲームがなめらかになることがあります。100%が実機と同じです。"},
    {"Dual core", "Doble núcleo", "Double cœur", "Dois núcleos", "Dual core", "デュアルコア"},
    {"Faster. Turn it off for a game that freezes or glitches.", "Más rápido. Desactívalo si un juego se congela o falla.", "Plus rapide. Désactive-le pour un jeu qui gèle ou bugue.", "Mais rápido. Desativa-o num jogo que congela ou falha.", "Più veloce. Disattivalo se un gioco si blocca o ha difetti.", "高速になります。ゲームがフリーズしたり表示が乱れたりする場合はオフにしてください。"},
    {"Fast disc loading", "Carga rápida del disco", "Chargement rapide du disque", "Carregamento rápido do disco", "Caricamento rapido del disco", "ディスク高速読み込み"},
    {"Shorter loading screens. A few games need real disc speed.", "Pantallas de carga más cortas. Algunos juegos necesitan la velocidad real.", "Des écrans de chargement plus courts. Quelques jeux exigent la vitesse réelle.", "Ecrãs de carregamento mais curtos. Alguns jogos precisam da velocidade real.", "Schermate di caricamento più brevi. Alcuni giochi richiedono la velocità reale del disco.", "ロード画面が短くなります。実際のディスク速度が必要なゲームもあります。"},
    {"Cheats", "Trucos", "Codes de triche", "Batotas", "Trucchi", "チート"},
    {"Dolphin's cheat codes for games that have them.", "Los trucos de Dolphin para los juegos que los tienen.", "Les codes de triche de Dolphin pour les jeux qui en ont.", "As batotas do Dolphin para os jogos que as têm.", "I codici trucco di Dolphin per i giochi che li hanno.", "対応するゲームで使える、Dolphinのチートコードです。"},
    {"System language", "Idioma de la consola", "Langue de la console", "Idioma da consola", "Lingua della console", "本体の言語"},
    {"The console's language. European games show their text in it.", "El idioma de la consola. Los juegos europeos muestran su texto en él.", "La langue de la console. Les jeux européens affichent leur texte dans celle-ci.", "O idioma da consola. Os jogos europeus mostram o texto nele.", "La lingua della console. I giochi europei mostrano i testi in questa lingua.", "本体の言語設定です。ヨーロッパ版のゲームはこの言語で文字を表示します。"},
    {"English", "Inglés", "Anglais", "Inglês", "Inglese", "英語"},
    {"Japanese", "Japonés", "Japonais", "Japonês", "Giapponese", "日本語"},
    {"German", "Alemán", "Allemand", "Alemão", "Tedesco", "ドイツ語"},
    {"French", "Francés", "Français", "Francês", "Francese", "フランス語"},
    {"Spanish", "Español", "Espagnol", "Espanhol", "Spagnolo", "スペイン語"},
    {"Italian", "Italiano", "Italien", "Italiano", "Italiano", "イタリア語"},
    {"Dutch", "Neerlandés", "Néerlandais", "Neerlandês", "Olandese", "オランダ語"},
    {"Chinese (simplified)", "Chino (simplificado)", "Chinois (simplifié)", "Chinês (simplificado)", "Cinese (semplificato)", "中国語（簡体字）"},
    {"Chinese (traditional)", "Chino (tradicional)", "Chinois (traditionnel)", "Chinês (tradicional)", "Cinese (tradizionale)", "中国語（繁体字）"},
    {"Korean", "Coreano", "Coréen", "Coreano", "Coreano", "韓国語"},
    {"Progressive scan", "Escaneo progresivo", "Balayage progressif", "Varrimento progressivo", "Scansione progressiva", "プログレッシブスキャン"},
    {"480p output, as on a component cable.", "Salida 480p, como con cable de componentes.", "Sortie 480p, comme avec un câble composante.", "Saída 480p, como com cabo de componentes.", "Uscita a 480p, come con un cavo component.", "コンポーネントケーブルと同じく、480pで出力します。"},

    /* Settings: Interface */
    {"Language", "Idioma", "Langue", "Idioma", "Lingua", "言語"},
    {"The language of Porpoise's menus. System follows your PS5.", "El idioma de los menús de Porpoise. Sistema sigue a tu PS5.", "La langue des menus de Porpoise. Système suit ta PS5.", "O idioma dos menus do Porpoise. Sistema segue a tua PS5.", "La lingua dei menu di Porpoise. Sistema segue la tua PS5.", "Porpoiseのメニューの言語です。「システム」はPS5の設定に合わせます。"},
    {"Reduced motion", "Menos movimiento", "Mouvements réduits", "Menos movimento", "Movimento ridotto", "動きを減らす"},
    {"Stops the moving lights and shortens animations.", "Detiene las luces en movimiento y acorta las animaciones.", "Arrête les lumières mobiles et raccourcit les animations.", "Para as luzes em movimento e encurta as animações.", "Ferma le luci in movimento e accorcia le animazioni.", "動く光を止め、アニメーションを短くします。"},
    {"Larger text", "Texto más grande", "Texte plus grand", "Texto maior", "Testo più grande", "文字を大きく"},
    {"Bigger labels across Porpoise.", "Textos más grandes en todo Porpoise.", "Des textes plus grands dans Porpoise.", "Textos maiores em todo o Porpoise.", "Etichette più grandi in tutto Porpoise.", "Porpoise全体の文字を大きくします。"},
    {"Reset all settings", "Restablecer todos los ajustes", "Réinitialiser tous les paramètres", "Repor todas as definições", "Ripristina tutte le impostazioni", "すべての設定をリセット"},
    {"Every setting back to how Porpoise ships. Games, folders and saves stay.", "Todos los ajustes vuelven a como vienen. Juegos, carpetas y partidas se quedan.", "Tous les paramètres reviennent à l'origine. Jeux, dossiers et sauvegardes restent.", "Todas as definições voltam ao original. Jogos, pastas e jogos guardados ficam.", "Tutte le impostazioni tornano a quelle originali di Porpoise. Giochi, cartelle e salvataggi restano.", "すべての設定を初期状態に戻します。ゲーム、フォルダー、セーブデータはそのまま残ります。"},
    {"Reset all settings?", "¿Restablecer todos los ajustes?", "Réinitialiser tous les paramètres ?", "Repor todas as definições?", "Ripristinare tutte le impostazioni?", "すべての設定をリセットしますか？"},
    {"Every setting goes back to how Porpoise ships. Your games, folders, covers and saves stay.", "Todos los ajustes vuelven a como vienen. Tus juegos, carpetas, portadas y partidas se quedan.", "Tous les paramètres reviennent à l'origine. Tes jeux, dossiers, jaquettes et sauvegardes restent.", "Todas as definições voltam ao original. Os teus jogos, pastas, capas e jogos guardados ficam.", "Tutte le impostazioni tornano a quelle originali di Porpoise. I tuoi giochi, cartelle, copertine e salvataggi restano.", "すべての設定が初期状態に戻ります。ゲーム、フォルダー、パッケージ画像、セーブデータはそのまま残ります。"},

    /* Settings: a game's own */
    {"Own settings", "Ajustes propios", "Paramètres propres", "Definições próprias", "Impostazioni proprie", "個別設定"},
    {"Changes here apply to this game only. Everything else follows your settings.", "Los cambios aquí son solo para este juego. Lo demás sigue tus ajustes.", "Les changements ici ne valent que pour ce jeu. Le reste suit tes paramètres.", "As alterações aqui só valem para este jogo. O resto segue as tuas definições.", "Le modifiche qui valgono solo per questo gioco. Tutto il resto segue le tue impostazioni.", "ここでの変更はこのゲームにだけ適用されます。それ以外は通常の設定に従います。"},
    {"None yet", "Ninguno aún", "Aucun pour l'instant", "Nenhuma ainda", "Ancora nessuna", "まだありません"},
    {"1 change", "1 cambio", "1 changement", "1 alteração", "1 modifica", "変更1件"},
    {"{n} changes", "{n} cambios", "{n} changements", "{n} alterações", "{n} modifiche", "変更{n}件"},
    {"Reset to default", "Restablecer", "Revenir par défaut", "Repor predefinição", "Ripristina predefiniti", "標準に戻す"},
    {"Forgets this game's own settings; it follows your settings again.", "Olvida los ajustes propios de este juego; vuelve a seguir tus ajustes.", "Oublie les paramètres propres à ce jeu ; il suit à nouveau les tiens.", "Esquece as definições deste jogo; volta a seguir as tuas.", "Dimentica le impostazioni proprie di questo gioco, che tornerà a seguire le tue impostazioni.", "このゲームの個別設定を消去し、通常の設定に戻します。"},
    {"Reset this game's settings?", "¿Restablecer los ajustes de este juego?", "Réinitialiser les paramètres de ce jeu ?", "Repor as definições deste jogo?", "Ripristinare le impostazioni di questo gioco?", "このゲームの設定をリセットしますか？"},
    {"It forgets its own settings and follows your settings again.", "Olvida sus ajustes propios y vuelve a seguir los tuyos.", "Il oublie ses paramètres propres et suit à nouveau les tiens.", "Esquece as suas definições e volta a seguir as tuas.", "Dimenticherà le sue impostazioni e tornerà a seguire le tue.", "個別設定を消去し、通常の設定に戻ります。"},

    /* Folder browser */
    {"Choose a game folder", "Elige una carpeta de juegos", "Choisis un dossier de jeux", "Escolhe uma pasta de jogos", "Scegli una cartella di giochi", "ゲームフォルダーを選択"},
    {"Pick a drive", "Elige una unidad", "Choisis un lecteur", "Escolhe uma unidade", "Scegli un'unità", "ドライブを選択"},
    {"Console storage", "Almacenamiento de la consola", "Stockage de la console", "Armazenamento da consola", "Memoria della console", "本体ストレージ"},
    {"USB drive {n}", "Unidad USB {n}", "Clé USB {n}", "Unidade USB {n}", "Unità USB {n}", "USBドライブ{n}"},
    {"Extended storage", "Almacenamiento ampliado", "Stockage étendu", "Armazenamento expandido", "Memoria espansa", "拡張ストレージ"},
    {"Whole system", "Todo el sistema", "Tout le système", "Todo o sistema", "Intero sistema", "システム全体"},
    {"No games right here", "No hay juegos justo aquí", "Pas de jeu ici même", "Sem jogos mesmo aqui", "Nessun gioco qui", "ここにゲームはありません"},
    {"1 game right here", "1 juego aquí", "1 jeu ici", "1 jogo aqui", "1 gioco qui", "ここにゲーム1本"},
    {"{n} games right here", "{n} juegos aquí", "{n} jeux ici", "{n} jogos aqui", "{n} giochi qui", "ここにゲーム{n}本"},
    {"No folders inside this one", "No hay carpetas dentro", "Aucun dossier à l'intérieur", "Sem pastas dentro desta", "Nessuna cartella qui dentro", "このフォルダー内にフォルダーはありません"},
    {"Use this folder", "Usar esta carpeta", "Utiliser ce dossier", "Usar esta pasta", "Usa questa cartella", "このフォルダーを使う"},
    {"Up", "Subir", "Remonter", "Subir", "Su", "上の階層へ"},

    /* About */
    {"Porpoise for PS5", "Porpoise para PS5", "Porpoise pour PS5", "Porpoise para PS5", "Porpoise per PS5", "Porpoise for PS5"},
    {"created by @elripalda", "creado por @elripalda", "créé par @elripalda", "criado por @elripalda", "creato da @elripalda", "制作：@elripalda"},
    {"Created by", "Creado por", "Créé par", "Criado por", "Creato da", "制作"},
    {"Website", "Sitio web", "Site web", "Site", "Sito web", "ウェブサイト"},
    {"Emulation", "Emulación", "Émulation", "Emulação", "Emulazione", "エミュレーション"},
    {"Dolphin core", "Núcleo de Dolphin", "Cœur Dolphin", "Núcleo do Dolphin", "Core Dolphin", "Dolphinコア"},
    {"Core API", "API del núcleo", "API du cœur", "API do núcleo", "API del core", "コアAPI"},
    {"PS5 graphics", "Gráficos de PS5", "Graphismes PS5", "Gráficos da PS5", "Grafica PS5", "PS5グラフィックス"},
    {"PS5 toolchain", "Herramientas PS5", "Outils PS5", "Ferramentas PS5", "Toolchain PS5", "PS5ツールチェーン"},
    {"PS5 port", "Port a PS5", "Portage PS5", "Port para PS5", "Port per PS5", "PS5移植"},
    {"Inspired by", "Inspirado en", "Inspiré par", "Inspirado em", "Ispirato a", "インスピレーション"},
    {"Box art and info", "Portadas e información", "Jaquettes et infos", "Capas e informação", "Copertine e info", "パッケージ画像と情報"},
    {"Font", "Fuente", "Police", "Tipo de letra", "Font", "フォント"},
    {"Images and audio", "Imágenes y audio", "Images et audio", "Imagens e áudio", "Immagini e audio", "画像とオーディオ"},
    {"Music and sounds", "Música y sonidos", "Musique et sons", "Música e sons", "Musica e suoni", "音楽と効果音"},
    {"The menu music and sound effects, made for Porpoise by Ruben.", "La música y los efectos del menú, hechos para Porpoise por Ruben.", "La musique et les effets du menu, créés pour Porpoise par Ruben.", "A música e os efeitos do menu, feitos para o Porpoise pelo Ruben.", "La musica e gli effetti sonori dei menu, creati per Porpoise da Ruben.", "メニューのBGMと効果音は、RubenがPorpoiseのために制作しました。"},
    {"Thanks", "Gracias", "Merci", "Agradecimentos", "Ringraziamenti", "謝辞"},
    {"Trademarks", "Marcas", "Marques", "Marcas", "Marchi", "商標"},
    {"A GameCube and Wii player for PS5, in the spirit of the GameCube's own menus.", "Un reproductor de GameCube y Wii para PS5, con el espíritu de los menús de GameCube.", "Un lecteur GameCube et Wii pour PS5, dans l'esprit des menus de la GameCube.", "Um leitor de GameCube e Wii para a PS5, no espírito dos menus da GameCube.", "Un lettore GameCube e Wii per PS5, nello spirito dei menu originali del GameCube.", "GameCubeのメニューの雰囲気を受け継いだ、PS5用のGameCube・Wiiプレイヤーです。"},
    {"Updates, news and more from the creator of Porpoise.", "Novedades y más del creador de Porpoise.", "Les nouveautés et plus encore du créateur de Porpoise.", "Novidades e mais do criador do Porpoise.", "Aggiornamenti, novità e altro dal creatore di Porpoise.", "Porpoise制作者のアップデート情報やニュースなど。"},
    {"PS5 homebrew front ends that showed the way.", "Interfaces homebrew de PS5 que marcaron el camino.", "Des interfaces homebrew PS5 qui ont montré la voie.", "Interfaces homebrew da PS5 que abriram caminho.", "Front end homebrew per PS5 che hanno aperto la strada.", "道を切り開いた、PS5のホームブリューのフロントエンドです。"},
    {"etaHEN, kstuff and ShadowMountPlus make homebrew like this possible.", "etaHEN, kstuff y ShadowMountPlus hacen posible el homebrew como este.", "etaHEN, kstuff et ShadowMountPlus rendent possible ce genre de homebrew.", "O etaHEN, o kstuff e o ShadowMountPlus tornam possível homebrew como este.", "etaHEN, kstuff e ShadowMountPlus rendono possibile un homebrew come questo.", "etaHEN、kstuff、ShadowMountPlusのおかげで、このようなホームブリューが実現しています。"},
    {"GameCube and Wii are trademarks of Nintendo. Porpoise is not affiliated with Nintendo.", "GameCube y Wii son marcas de Nintendo. Porpoise no está afiliado a Nintendo.", "GameCube et Wii sont des marques de Nintendo. Porpoise n'est pas affilié à Nintendo.", "GameCube e Wii são marcas da Nintendo. O Porpoise não é afiliado da Nintendo.", "GameCube e Wii sono marchi di Nintendo. Porpoise non è affiliato a Nintendo.", "GameCubeおよびWiiはNintendoの商標です。PorpoiseはNintendoとは関係ありません。"},
    {"Covers, disc art and game details from GameTDB.com and its contributors.", "Portadas, arte de discos y detalles de GameTDB.com y sus colaboradores.", "Jaquettes, images de disque et détails de GameTDB.com et de ses contributeurs.", "Capas, arte de discos e detalhes do GameTDB.com e dos seus colaboradores.", "Copertine, immagini dei dischi e dettagli dei giochi da GameTDB.com e dai suoi collaboratori.", "パッケージ画像、ディスク画像、ゲーム情報は、GameTDB.comとその協力者によるものです。"},
    {"Porpoise hosts the core through the libretro API that RetroArch made.", "Porpoise ejecuta el núcleo con la API libretro creada por RetroArch.", "Porpoise fait tourner le cœur via l'API libretro créée par RetroArch.", "O Porpoise corre o núcleo com a API libretro criada pelo RetroArch.", "Porpoise esegue il core tramite l'API libretro creata da RetroArch.", "Porpoiseは、RetroArchが作ったlibretro APIを通じてコアを動かしています。"},
    {"The PS5 Dolphin and RetroArch port work Porpoise is built on.", "El trabajo de port de Dolphin y RetroArch a PS5 sobre el que se construye Porpoise.", "Le travail de portage de Dolphin et RetroArch sur PS5 sur lequel repose Porpoise.", "O trabalho de port do Dolphin e do RetroArch para a PS5 em que o Porpoise assenta.", "Il lavoro di port di Dolphin e RetroArch su PS5 su cui si basa Porpoise.", "Porpoiseの土台となった、DolphinとRetroArchのPS5移植です。"},

    /* Covers status (main) */
    {"Getting game info", "Descargando información", "Récupération des infos", "A obter informação", "Download info giochi", "ゲーム情報を取得中"},
    {"Getting covers {done} of {total}", "Descargando portadas {done} de {total}", "Jaquettes {done} sur {total}", "A obter capas {done} de {total}", "Download copertine {done} di {total}", "パッケージ画像を取得中 {done}/{total}"},
    {"Getting disc art {done} of {total}", "Descargando discos {done} de {total}", "Images de disque {done} sur {total}", "A obter discos {done} de {total}", "Download immagini dischi {done} di {total}", "ディスク画像を取得中 {done}/{total}"},

    /* 1.1: layouts, the in-game menu, save states, filters, borders, recommended settings, favourites */
    {"1 saved", "1 guardado", "1 sauvegardé", "1 guardado", "1 salvato", "1件セーブ済み"},
    {"5x (1800p) • exp.", "5x (1800p) • exp.", "5x (1800p) • exp.", "5x (1800p) • exp.", "5x (1800p) • sper.", "5x (1800p) • 実験的"},
    {"6x (4K) • exp.", "6x (4K) • exp.", "6x (4K) • exp.", "6x (4K) • exp.", "6x (4K) • sper.", "6x (4K) • 実験的"},
    {"Accurate (LLE)", "Preciso (LLE)", "Précis (LLE)", "Preciso (LLE)", "Accurata (LLE)", "正確（LLE）"},
    {"Accurate NaNs", "NaN precisos", "NaN précis", "NaN precisos", "NaN accurati", "正確なNaN処理"},
    {"Arbitrary mipmap detection", "Detección de mipmaps arbitrarios", "Détection des mipmaps arbitraires", "Deteção de mipmaps arbitrários", "Rilevamento mipmap arbitrarie", "任意のミップマップを検出"},
    {"Arcade CRT", "CRT arcade", "CRT arcade", "CRT arcade", "CRT arcade", "アーケードCRT"},
    {"Arcade cabinet", "Máquina recreativa", "Borne d'arcade", "Máquina de arcada", "Cabinato arcade", "アーケード台"},
    {"Async ubershaders hide the stutter when a game draws something new. Takes effect after a restart of the game.", "Los ubershaders asíncronos evitan tirones cuando el juego dibuja algo nuevo. Se aplica al reiniciar el juego.", "Les ubershaders asynchrones évitent les saccades quand le jeu affiche du nouveau. Prend effet après un redémarrage du jeu.", "Os ubershaders assíncronos evitam soluços quando o jogo desenha algo novo. Aplica-se depois de reiniciar o jogo.", "Gli ubershader asincroni nascondono gli scatti quando un gioco disegna qualcosa di nuovo. Vale dopo il riavvio del gioco.", "非同期Ubershadersは、ゲームが新しいものを描画するときのカクつきを抑えます。ゲームを再起動すると反映されます。"},
    {"Audio emulation", "Emulación de audio", "Émulation audio", "Emulação de áudio", "Emulazione audio", "オーディオエミュレーション"},
    {"Border", "Marco", "Bordure", "Moldura", "Bordo", "ボーダー"},
    {"Bounding box", "Bounding box", "Bounding box", "Bounding box", "Bounding box", "バウンディングボックス"},
    {"C-stick", "Stick C", "Stick C", "Stick C", "C-stick", "Cスティック"},
    {"CPU access to the EFB", "Acceso de la CPU al EFB", "Accès du CPU à l'EFB", "Acesso do CPU ao EFB", "Accesso della CPU all'EFB", "EFBへのCPUアクセス"},
    {"CPU overclock", "Overclock de CPU", "Overclock du CPU", "Overclock do CPU", "Overclock CPU", "CPUオーバークロック"},
    {"CPU recompiler", "Recompilador de CPU", "Recompilateur CPU", "Recompilador do CPU", "Ricompilatore CPU", "CPUリコンパイラー"},
    {"Control stick", "Stick de control", "Stick de contrôle", "Stick de controlo", "Stick di controllo", "コントロールスティック"},
    {"Controller art", "Arte del mando", "Images de la manette", "Arte do comando", "Immagine controller", "コントローラーのイラスト"},
    {"Cull vertices on the CPU", "Descartar vértices en la CPU", "Éliminer les sommets sur le CPU", "Descartar vértices no CPU", "Scarta vertici sulla CPU", "CPUで頂点をカリング"},
    {"Defer EFB copies", "Aplazar copias del EFB", "Différer les copies EFB", "Adiar cópias do EFB", "Rinvia copie EFB", "EFBコピーを遅延"},
    {"Delete this save state?", "¿Borrar este estado guardado?", "Supprimer cet état sauvegardé ?", "Apagar este estado guardado?", "Eliminare questo stato salvato?", "このステートセーブを削除しますか？"},
    {"Dolphin has no fixes listed for this game, and Porpoise has no picks for it yet. The list grows as games are tested.", "Dolphin no tiene arreglos para este juego y Porpoise aún no tiene una selección para él. La lista crece a medida que se prueban juegos.", "Dolphin n'a aucun correctif pour ce jeu, et Porpoise n'a pas encore de choix pour lui. La liste s'allonge à mesure que les jeux sont testés.", "O Dolphin não tem correções para este jogo e o Porpoise ainda não tem escolhas para ele. A lista cresce à medida que os jogos são testados.", "Dolphin non ha correzioni per questo gioco e Porpoise non ha ancora consigli. L'elenco cresce man mano che i giochi vengono testati.", "Dolphinにはこのゲーム用の修正が登録されておらず、Porpoiseのおすすめもまだありません。ゲームの検証が進むにつれてリストは増えていきます。"},
    {"Dolphin's fix: {why}", "Arreglo de Dolphin: {why}", "Correctif de Dolphin : {why}", "Correção do Dolphin: {why}", "Correzione di Dolphin: {why}", "Dolphinの修正：{why}"},
    {"Dolphin's libretro core, maintained by the libretro team (GPL v2 or later).", "El núcleo libretro de Dolphin, mantenido por el equipo de libretro (GPL v2 o posterior).", "Le cœur libretro de Dolphin, maintenu par l'équipe libretro (GPL v2 ou ultérieure).", "O núcleo libretro do Dolphin, mantido pela equipa libretro (GPL v2 ou posterior).", "Il core libretro di Dolphin, mantenuto dal team libretro (GPL v2 o successiva).", "libretroチームが管理する、Dolphinのlibretroコアです（GPL v2以降）。"},
    {"Dolphin, by the Dolphin Team - dolphin-emu.org. Free software, GPL v2 or later.", "Dolphin, del equipo de Dolphin - dolphin-emu.org. Software libre, GPL v2 o posterior.", "Dolphin, par l'équipe Dolphin - dolphin-emu.org. Logiciel libre, GPL v2 ou ultérieure.", "Dolphin, da equipa do Dolphin - dolphin-emu.org. Software livre, GPL v2 ou posterior.", "Dolphin, del Dolphin Team - dolphin-emu.org. Software libero, GPL v2 o successiva.", "Dolphin：Dolphin Team制作 - dolphin-emu.org。フリーソフトウェア（GPL v2以降）。"},
    {"Download it from github.com/elripalda/Porpoise/releases. Delete the old PPSA99764 folder, then copy the new one in its place; your games, saves and settings stay.", "Descárgala de github.com/elripalda/Porpoise/releases. Borra la carpeta PPSA99764 antigua y copia la nueva en su lugar; tus juegos, partidas y ajustes se quedan.", "Télécharge-la sur github.com/elripalda/Porpoise/releases. Supprime l'ancien dossier PPSA99764, puis copie le nouveau à sa place ; tes jeux, sauvegardes et paramètres restent.", "Transfere-a de github.com/elripalda/Porpoise/releases. Apaga a pasta PPSA99764 antiga e copia a nova para o seu lugar; os teus jogos, jogos guardados e definições ficam.", "Scaricala da github.com/elripalda/Porpoise/releases. Elimina la vecchia cartella PPSA99764, poi copia quella nuova al suo posto; giochi, salvataggi e impostazioni restano.", "github.com/elripalda/Porpoise/releasesからダウンロードしてください。古いPPSA99764フォルダーを削除してから、新しいフォルダーを同じ場所にコピーします。ゲーム、セーブデータ、設定はそのまま残ります。"},
    {"EFB copies", "Copias del EFB", "Copies EFB", "Cópias do EFB", "Copie EFB", "EFBコピー"},
    {"Early XFB output", "Salida temprana del XFB", "Sortie XFB anticipée", "Saída antecipada do XFB", "Uscita XFB anticipata", "XFBの早期出力"},
    {"Empty", "Vacía", "Vide", "Vazia", "Vuoto", "空き"},
    {"Emulate EFB format changes", "Emular cambios de formato del EFB", "Émuler les changements de format EFB", "Emular alterações de formato do EFB", "Emula cambi di formato EFB", "EFBフォーマット変更をエミュレート"},
    {"Every game: {layout}", "Todos los juegos: {layout}", "Tous les jeux : {layout}", "Todos os jogos: {layout}", "Tutti i giochi: {layout}", "すべてのゲーム：{layout}"},
    {"Fast (HLE)", "Rápido (HLE)", "Rapide (HLE)", "Rápido (HLE)", "Veloce (HLE)", "高速（HLE）"},
    {"Fast depth calculation", "Cálculo rápido de profundidad", "Calcul rapide de la profondeur", "Cálculo rápido de profundidade", "Calcolo rapido della profondità", "高速深度計算"},
    {"Fast forward", "Avance rápido", "Avance rapide", "Avanço rápido", "Avanti veloce", "早送り"},
    {"Fast texture sampling", "Muestreo rápido de texturas", "Échantillonnage rapide des textures", "Amostragem rápida de texturas", "Campionamento rapido texture", "高速テクスチャーサンプリング"},
    {"Favourite", "Favorito", "Favori", "Favorito", "Preferito", "お気に入りに追加"},
    {"Favourites first", "Favoritos primero", "Favoris d'abord", "Favoritos primeiro", "Prima i preferiti", "お気に入り順"},
    {"Fills the bars beside a 4:3 picture (widescreen off). Add your own 1920x1080 PNGs to /data/porpoise/borders.", "Rellena las barras a los lados de una imagen 4:3 (pantalla ancha desactivada). Añade tus propios PNG de 1920x1080 en /data/porpoise/borders.", "Remplit les bandes à côté d'une image 4:3 (écran large désactivé). Ajoute tes propres PNG 1920x1080 dans /data/porpoise/borders.", "Preenche as barras ao lado de uma imagem 4:3 (ecrã panorâmico desligado). Adiciona os teus próprios PNG de 1920x1080 em /data/porpoise/borders.", "Riempie le bande ai lati di un'immagine 4:3 (widescreen disattivato). Aggiungi i tuoi PNG 1920x1080 in /data/porpoise/borders.", "4:3の映像の横の帯を埋めます（ワイドスクリーンハックがオフのとき）。/data/porpoise/bordersに1920x1080のPNGを追加すると、自分の画像も使えます。"},
    {"Fills the bars beside a 4:3 picture (widescreen off). Add your own PNGs to /data/porpoise/borders.", "Rellena las barras a los lados de una imagen 4:3 (pantalla ancha desactivada). Añade tus propios PNG en /data/porpoise/borders.", "Remplit les bandes à côté d'une image 4:3 (écran large désactivé). Ajoute tes propres PNG dans /data/porpoise/borders.", "Preenche as barras ao lado de uma imagem 4:3 (ecrã panorâmico desligado). Adiciona os teus próprios PNG em /data/porpoise/borders.", "Riempie le bande ai lati di un'immagine 4:3 (widescreen disattivato). Aggiungi i tuoi PNG in /data/porpoise/borders.", "4:3の映像の横の帯を埋めます（ワイドスクリーンハックがオフのとき）。/data/porpoise/bordersにPNGを追加すると、自分の画像も使えます。"},
    {"Filter strength", "Intensidad del filtro", "Intensité du filtre", "Intensidade do filtro", "Intensità filtro", "フィルターの強さ"},
    {"Floating-point result flags", "Indicadores de resultado de coma flotante", "Indicateurs de résultat en virgule flottante", "Indicadores de resultado de vírgula flutuante", "Flag risultato in virgola mobile", "浮動小数点結果フラグ"},
    {"Found a problem? Scan the code with your phone and open an issue with the game, what happened and porpoise/core.log.", "¿Algo falla? Escanea el código con tu móvil y abre un issue con el juego, lo que pasó y porpoise/core.log.", "Un problème ? Scanne le code avec ton téléphone et ouvre un ticket avec le jeu, ce qui s'est passé et porpoise/core.log.", "Algo correu mal? Lê o código com o telemóvel e abre um issue com o jogo, o que aconteceu e o porpoise/core.log.", "Hai trovato un problema? Inquadra il codice con il telefono e apri una segnalazione con il gioco, cosa è successo e porpoise/core.log.", "問題がありましたか？スマートフォンでコードを読み取り、ゲーム名、起きたこと、porpoise/core.logを添えてIssueを作成してください。"},
    {"Full memory management (MMU)", "Gestión completa de memoria (MMU)", "Gestion complète de la mémoire (MMU)", "Gestão completa da memória (MMU)", "Gestione memoria completa (MMU)", "完全なメモリー管理（MMU）"},
    {"Game", "Juego", "Jeu", "Jogo", "Gioco", "ゲーム"},
    {"GameCube: Cross is A, Square is B. PlayStation: Cross is A, Circle is B. My layouts: your own, made in Customize buttons.", "GameCube: X es A y cuadrado es B. PlayStation: X es A y círculo es B. Mis distribuciones: las tuyas, creadas en Personalizar botones.", "GameCube : Croix est A, Carré est B. PlayStation : Croix est A, Rond est B. Mes dispositions : les tiennes, créées dans Personnaliser les boutons.", "GameCube: X é A e quadrado é B. PlayStation: X é A e círculo é B. As minhas disposições: as tuas, criadas em Personalizar botões.", "GameCube: Croce è A, Quadrato è B. PlayStation: Croce è A, Cerchio è B. Le mie configurazioni: le tue, create in Personalizza tasti.", "GameCube：×がA、□がBです。PlayStation：×がA、○がBです。マイ配置：「ボタンをカスタマイズ」で作った自分の配置です。"},
    {"How strong the screen filter is.", "Qué tan fuerte es el filtro de pantalla.", "L'intensité du filtre d'écran.", "A intensidade do filtro de ecrã.", "Quanto è intenso il filtro schermo.", "画面フィルターの強さです。"},
    {"Instruction cache", "Caché de instrucciones", "Cache d'instructions", "Cache de instruções", "Cache istruzioni", "命令キャッシュ"},
    {"John Törnblom's ps5-payload-sdk (GPL v3) and its PS5 ports.", "El ps5-payload-sdk de John Törnblom (GPL v3) y sus ports a PS5.", "Le ps5-payload-sdk de John Törnblom (GPL v3) et ses portages PS5.", "O ps5-payload-sdk de John Törnblom (GPL v3) e os seus ports para a PS5.", "ps5-payload-sdk di John Törnblom (GPL v3) e i suoi port per PS5.", "John Törnblomのps5-payload-sdk（GPL v3）と、そのPS5移植。"},
    {"Layout", "Distribución", "Disposition", "Disposição", "Configurazione", "配置"},
    {"Less than a minute", "Menos de un minuto", "Moins d'une minute", "Menos de um minuto", "Meno di un minuto", "1分未満"},
    {"Load", "Cargar", "Charger", "Carregar", "Carica", "ロード"},
    {"Load state", "Cargar estado", "Charger l'état", "Carregar estado", "Carica stato", "ステートロード"},
    {"Low DCBZ hack", "Hack Low DCBZ", "Hack Low DCBZ", "Hack Low DCBZ", "Hack DCBZ basso", "Low DCBZハック"},
    {"Make up to four layouts of your own, on a picture of the controller. Any game can use any of them.", "Crea hasta cuatro distribuciones propias sobre un dibujo del mando. Cualquier juego puede usar cualquiera de ellas.", "Crée jusqu'à quatre dispositions à toi, sur un dessin de la manette. Chaque jeu peut utiliser n'importe laquelle.", "Cria até quatro disposições tuas, num desenho do comando. Qualquer jogo pode usar qualquer uma delas.", "Crea fino a quattro configurazioni tue, su un'immagine del controller. Ogni gioco può usarne una qualsiasi.", "コントローラーの画像を見ながら、自分の配置を最大4つまで作れます。どのゲームでも使えます。"},
    {"Memory card size", "Tamaño de la tarjeta de memoria", "Taille de la carte mémoire", "Tamanho do cartão de memória", "Dimensione Memory Card", "メモリーカードの容量"},
    {"Midnight", "Medianoche", "Minuit", "Meia-noite", "Mezzanotte", "ミッドナイト"},
    {"Most played", "Más jugados", "Les plus joués", "Mais jogados", "Più giocati", "プレイ時間順"},
    {"My layout {n}", "Mi distribución {n}", "Ma disposition {n}", "A minha disposição {n}", "Mia configurazione {n}", "マイ配置{n}"},
    {"None", "Ninguno", "Aucun", "Nenhum", "Nessuno", "なし"},
    {"Nothing needed", "No hace falta nada", "Rien à faire", "Não é preciso nada", "Niente da cambiare", "調整不要"},
    {"Nunito by Vernon Adams and contributors, SIL Open Font License.", "Nunito, de Vernon Adams y colaboradores, SIL Open Font License.", "Nunito par Vernon Adams et ses contributeurs, SIL Open Font License.", "Nunito, de Vernon Adams e colaboradores, SIL Open Font License.", "Nunito di Vernon Adams e collaboratori, SIL Open Font License.", "Nunito：Vernon Adamsほか制作、SIL Open Font License。"},
    {"One of Dolphin's own fixes for this game. Dolphin applies it by itself.", "Uno de los arreglos propios de Dolphin para este juego. Dolphin lo aplica solo.", "Un des correctifs de Dolphin pour ce jeu. Dolphin l'applique tout seul.", "Uma das correções do próprio Dolphin para este jogo. O Dolphin aplica-a sozinho.", "Una delle correzioni di Dolphin per questo gioco. Dolphin la applica da solo.", "Dolphinがこのゲーム用に用意している修正の1つです。Dolphinが自動で適用します。"},
    {"PS5 Button Icons and Controls by Zacksly - zacksly.itch.io, @_Zacksly on Twitter. CC BY 3.0, adapted for Porpoise.", "PS5 Button Icons and Controls de Zacksly - zacksly.itch.io, @_Zacksly en Twitter. CC BY 3.0, adaptado para Porpoise.", "PS5 Button Icons and Controls par Zacksly - zacksly.itch.io, @_Zacksly sur Twitter. CC BY 3.0, adapté pour Porpoise.", "PS5 Button Icons and Controls de Zacksly - zacksly.itch.io, @_Zacksly no Twitter. CC BY 3.0, adaptado para o Porpoise.", "PS5 Button Icons and Controls di Zacksly - zacksly.itch.io, @_Zacksly su Twitter. CC BY 3.0, adattate per Porpoise.", "PS5 Button Icons and Controls：Zacksly制作 - zacksly.itch.io、Twitterの@_Zacksly。CC BY 3.0、Porpoise向けに改変。"},
    {"Performance queries", "Consultas de rendimiento", "Requêtes de performance", "Consultas de desempenho", "Query di prestazioni", "パフォーマンスクエリ"},
    {"Play from here", "Jugar desde aquí", "Jouer à partir d'ici", "Jogar a partir daqui", "Gioca da qui", "ここからプレイ"},
    {"Play time", "Tiempo de juego", "Temps de jeu", "Tempo de jogo", "Tempo di gioco", "プレイ時間"},
    {"PlayStation, PS5 and DualSense are trademarks of Sony Interactive Entertainment. Porpoise is not affiliated with Sony.", "PlayStation, PS5 y DualSense son marcas de Sony Interactive Entertainment. Porpoise no está afiliado a Sony.", "PlayStation, PS5 et DualSense sont des marques de Sony Interactive Entertainment. Porpoise n'est pas affilié à Sony.", "PlayStation, PS5 e DualSense são marcas da Sony Interactive Entertainment. O Porpoise não é afiliado da Sony.", "PlayStation, PS5 e DualSense sono marchi di Sony Interactive Entertainment. Porpoise non è affiliato a Sony.", "PlayStation、PS5、DualSenseはSony Interactive Entertainmentの商標です。PorpoiseはSonyとは関係ありません。"},
    {"Porpoise glass", "Cristal Porpoise", "Verre Porpoise", "Vidro Porpoise", "Vetro Porpoise", "Porpoiseガラス"},
    {"Porpoise {version} is out", "Ya está aquí Porpoise {version}", "Porpoise {version} est sorti", "Já saiu o Porpoise {version}", "È uscito Porpoise {version}", "Porpoise {version}が公開されました"},
    {"Porpoise's own filter on the way to the TV. CRT and Arcade CRT look best at 1080p or above.", "El filtro propio de Porpoise de camino a la TV. CRT y CRT arcade se ven mejor a 1080p o más.", "Le filtre de Porpoise sur le chemin de la TV. CRT et CRT arcade rendent mieux en 1080p ou plus.", "O filtro do próprio Porpoise a caminho da TV. CRT e CRT arcade ficam melhor a 1080p ou mais.", "Il filtro di Porpoise applicato verso la TV. CRT e CRT arcade rendono al meglio a 1080p o più.", "テレビに出力する前にかける、Porpoise独自のフィルターです。CRTとアーケードCRTは1080p以上で最もきれいに見えます。"},
    {"Porpoise's own filter on the way to the TV: smooth or sharp scaling, sharpening, a CRT, an arcade monitor or a worn VHS tape.", "El filtro propio de Porpoise de camino a la TV: escalado suave o nítido, enfoque, un CRT, un monitor arcade o una cinta VHS gastada.", "Le filtre de Porpoise sur le chemin de la TV : mise à l'échelle lisse ou nette, netteté, un CRT, un moniteur d'arcade ou une cassette VHS usée.", "O filtro do próprio Porpoise a caminho da TV: escala suave ou nítida, nitidez, um CRT, um monitor de arcada ou uma cassete VHS gasta.", "Il filtro di Porpoise applicato verso la TV: scaling morbido o nitido, nitidezza, un CRT, un monitor arcade o una vecchia cassetta VHS.", "テレビに出力する前にかける、Porpoise独自のフィルターです。なめらかな拡大、くっきりした拡大、シャープ化、CRT、アーケードモニター、劣化したVHSテープから選べます。"},
    {"Dolphin's own fixes for this game, and Porpoise's picks", "Los arreglos de Dolphin para este juego y la selección de Porpoise", "Les correctifs de Dolphin pour ce jeu et les choix de Porpoise", "As correções do Dolphin para este jogo e as escolhas do Porpoise", "Le correzioni di Dolphin per questo gioco e i consigli di Porpoise", "Dolphinによるこのゲームの修正とPorpoiseのおすすめ"},
    {"Porpoise's picks", "Selección de Porpoise", "Les choix de Porpoise", "Escolhas do Porpoise", "Consigli di Porpoise", "Porpoiseのおすすめ"},
    {"Recommended", "Recomendado", "Recommandé", "Recomendado", "Consigliato", "おすすめ"},
    {"Recommended settings", "Ajustes recomendados", "Paramètres recommandés", "Definições recomendadas", "Impostazioni consigliate", "おすすめ設定"},
    {"Runs the game 2x or 4x faster until you turn it off. The game's sound pauses meanwhile.", "Acelera el juego 2x o 4x hasta que lo desactives. Mientras tanto, el sonido del juego se pausa.", "Fait tourner le jeu 2x ou 4x plus vite jusqu'à ce que tu le désactives. Le son du jeu est en pause pendant ce temps.", "Põe o jogo 2x ou 4x mais rápido até o desligares. Entretanto, o som do jogo fica em pausa.", "Fa andare il gioco 2x o 4x più veloce finché non lo disattivi. Nel frattempo l'audio del gioco va in pausa.", "オフにするまで、ゲームを2倍または4倍の速さで進めます。その間、ゲームの音は止まります。"},
    {"Save", "Guardar", "Sauvegarder", "Guardar", "Salva", "セーブ"},
    {"Save a state from the in-game menu (OPTIONS + touch pad) while you play.", "Guarda un estado desde el menú del juego (OPTIONS + panel táctil) mientras juegas.", "Sauvegarde un état depuis le menu en jeu (OPTIONS + pavé tactile) pendant que tu joues.", "Guarda um estado no menu do jogo (OPTIONS + painel tátil) enquanto jogas.", "Salva uno stato dal menu di gioco (OPTIONS + touchpad) mentre giochi.", "プレイ中にゲーム内メニュー（OPTIONS + タッチパッド）からステートセーブできます。"},
    {"Save state", "Guardar estado", "Sauvegarder l'état", "Guardar estado", "Salva stato", "ステートセーブ"},
    {"Save states", "Estados guardados", "États sauvegardés", "Estados guardados", "Stati salvati", "ステートセーブ"},
    {"Saved", "Guardado", "Sauvegardé", "Guardado", "Salvato", "セーブ済み"},
    {"Saved to slot {n}.", "Guardado en la ranura {n}.", "Sauvegardé dans l'emplacement {n}.", "Guardado na ranhura {n}.", "Salvato nello slot {n}.", "スロット{n}にセーブしました。"},
    {"Screen filter", "Filtro de pantalla", "Filtre d'écran", "Filtro de ecrã", "Filtro schermo", "画面フィルター"},
    {"Settings > About", "Ajustes > Acerca de", "Paramètres > À propos", "Definições > Sobre", "Impostazioni > Info", "設定 > 情報"},
    {"Settings that run this game best on PS5, as tested.", "Los ajustes con los que este juego funciona mejor en PS5, según las pruebas.", "Les paramètres qui font le mieux tourner ce jeu sur PS5, d'après les tests.", "As definições com que este jogo corre melhor na PS5, segundo os testes.", "Le impostazioni con cui questo gioco gira meglio su PS5, secondo i test.", "PS5でこのゲームを最適に動かすための、検証済みの設定です。"},
    {"Sharpen", "Enfocar", "Netteté", "Nitidez", "Nitidezza", "シャープ化"},
    {"Show frames immediately (XFB)", "Mostrar fotogramas al instante (XFB)", "Afficher les images immédiatement (XFB)", "Mostrar imagens de imediato (XFB)", "Mostra subito i fotogrammi (XFB)", "フレームを即時表示（XFB）"},
    {"Skip VI interrupts", "Omitir interrupciones VI", "Ignorer les interruptions VI", "Saltar interrupções VI", "Salta interrupt VI", "VI割り込みをスキップ"},
    {"Slot {n}", "Ranura {n}", "Emplacement {n}", "Ranhura {n}", "Slot {n}", "スロット{n}"},
    {"Slot {n} loaded.", "Ranura {n} cargada.", "Emplacement {n} chargé.", "Ranhura {n} carregada.", "Slot {n} caricato.", "スロット{n}をロードしました。"},
    {"Slot {n} of {game} will be deleted. Your memory card saves are not touched.", "Se borrará la ranura {n} de {game}. Las partidas de tu tarjeta de memoria no se tocan.", "L'emplacement {n} de {game} sera supprimé. Les sauvegardes de ta carte mémoire ne sont pas touchées.", "A ranhura {n} de {game} vai ser apagada. Os jogos guardados no teu cartão de memória não são afetados.", "Lo slot {n} di {game} verrà eliminato. I salvataggi della Memory Card restano intatti.", "{game}のスロット{n}を削除します。メモリーカードのセーブデータには影響しません。"},
    {"Start over", "Empezar de nuevo", "Recommencer", "Recomeçar", "Ricomincia", "最初から"},
    {"Start over from the GameCube layout", "Empezar de nuevo desde la distribución GameCube", "Recommencer depuis la disposition GameCube", "Recomeçar a partir da disposição GameCube", "Ricomincia dalla configurazione GameCube", "GameCube配置から作り直す"},
    {"Start over from the PlayStation layout", "Empezar de nuevo desde la distribución PlayStation", "Recommencer depuis la disposition PlayStation", "Recomeçar a partir da disposição PlayStation", "Ricomincia dalla configurazione PlayStation", "PlayStation配置から作り直す"},
    {"Sync on idle skipping", "Sincronizar al omitir inactividad", "Synchroniser au saut d'inactivité", "Sincronizar ao saltar inatividade", "Sincronizza durante l'idle skipping", "アイドルスキップ時に同期"},
    {"Tabs", "Pestañas", "Onglets", "Separadores", "Schede", "タブ"},
    {"That slot couldn't be loaded.", "No se pudo cargar esa ranura.", "Impossible de charger cet emplacement.", "Não foi possível carregar essa ranhura.", "Impossibile caricare quello slot.", "そのスロットをロードできませんでした。"},
    {"That slot is empty.", "Esa ranura está vacía.", "Cet emplacement est vide.", "Essa ranhura está vazia.", "Quello slot è vuoto.", "そのスロットは空です。"},
    {"The game's state couldn't be saved.", "No se pudo guardar el estado del juego.", "Impossible de sauvegarder l'état du jeu.", "Não foi possível guardar o estado do jogo.", "Impossibile salvare lo stato del gioco.", "ゲームの状態をセーブできませんでした。"},
    {"Three slots for this game. Left and right choose the slot.", "Tres ranuras para este juego. Izquierda y derecha eligen la ranura.", "Trois emplacements pour ce jeu. Gauche et droite choisissent l'emplacement.", "Três ranhuras para este jogo. Esquerda e direita escolhem a ranhura.", "Tre slot per questo gioco. Scegli lo slot con sinistra e destra.", "このゲームには3つのスロットがあります。左右でスロットを選びます。"},
    {"To RAM too (accurate)", "También a la RAM (preciso)", "Aussi vers la RAM (précis)", "Também para a RAM (preciso)", "Anche in RAM (accurato)", "RAMにも（正確）"},
    {"To texture only", "Solo a textura", "Vers texture seulement", "Só para textura", "Solo su texture", "テクスチャーのみ"},
    {"Unfavourite", "Quitar favorito", "Retirer des favoris", "Remover favorito", "Rimuovi dai preferiti", "お気に入りから削除"},
    {"Update available", "Actualización disponible", "Mise à jour disponible", "Atualização disponível", "Aggiornamento disponibile", "アップデートがあります"},
    {"Use this layout", "Usar esta distribución", "Utiliser cette disposition", "Usar esta disposição", "Usa questa configurazione", "この配置を使う"},
    {"Vertex rounding", "Redondeo de vértices", "Arrondi des sommets", "Arredondamento de vértices", "Arrotondamento vertici", "頂点の丸め"},
    {"Vulkan on PS5 through Mesa's RADV driver (MIT), PS5_Mesa and PS5_Vulkan ports.", "Vulkan en PS5 mediante el driver RADV de Mesa (MIT), ports PS5_Mesa y PS5_Vulkan.", "Vulkan sur PS5 via le pilote RADV de Mesa (MIT), portages PS5_Mesa et PS5_Vulkan.", "Vulkan na PS5 através do driver RADV do Mesa (MIT), ports PS5_Mesa e PS5_Vulkan.", "Vulkan su PS5 tramite il driver RADV di Mesa (MIT) e i port PS5_Mesa e PS5_Vulkan.", "MesaのRADVドライバー（MIT）と、PS5_Mesa・PS5_Vulkanの移植によるPS5上のVulkan。"},
    {"XFB copies", "Copias del XFB", "Copies XFB", "Cópias do XFB", "Copie XFB", "XFBコピー"},
    {"Your own layouts: change any button on a picture of the DualSense.", "Tus propias distribuciones: cambia cualquier botón sobre un dibujo del DualSense.", "Tes propres dispositions : change n'importe quel bouton sur un dessin de la DualSense.", "As tuas próprias disposições: muda qualquer botão num desenho do DualSense.", "Le tue configurazioni: cambia qualsiasi tasto su un'immagine del DualSense.", "自分の配置：DualSenseの画像を見ながら、好きなボタンを変更できます。"},
    {"stb_image, stb_truetype and stb_vorbis by Sean Barrett (public domain / MIT).", "stb_image, stb_truetype y stb_vorbis de Sean Barrett (dominio público / MIT).", "stb_image, stb_truetype et stb_vorbis par Sean Barrett (domaine public / MIT).", "stb_image, stb_truetype e stb_vorbis de Sean Barrett (domínio público / MIT).", "stb_image, stb_truetype e stb_vorbis di Sean Barrett (pubblico dominio / MIT).", "Sean Barrettによるstb_image、stb_truetype、stb_vorbis（パブリックドメイン / MIT）。"},
    {"{h} h {m} min", "{h} h {m} min", "{h} h {m} min", "{h} h {m} min", "{h} h {m} min", "{h}時間{m}分"},
    {"{layout} is on for this game.", "{layout} está activa en este juego.", "{layout} est active pour ce jeu.", "{layout} está ativa neste jogo.", "{layout} è attiva per questo gioco.", "このゲームでは{layout}を使用中です。"},
    {"{layout} is on.", "{layout} está activa.", "{layout} est active.", "{layout} está ativa.", "{layout} è attiva.", "{layout}を使用中です。"},
    {"{layout} now starts from the {base} layout.", "{layout} ahora parte de la distribución {base}.", "{layout} part maintenant de la disposition {base}.", "{layout} parte agora da disposição {base}.", "{layout} ora parte dalla configurazione {base}.", "{layout}を{base}配置から作り直しました。"},
    {"{m} min", "{m} min", "{m} min", "{m} min", "{m} min", "{m}分"},
    {"{n} saved", "{n} guardados", "{n} sauvegardés", "{n} guardados", "{n} salvati", "{n}件セーブ済み"},
    {"Use…", "Usar…", "Utiliser…", "Usar…", "Usa…", "使う…"},
};

const char *const kSystemChoice[] = {"System", "Sistema", "Système", "Sistema", "Sistema", "本体の設定"};
/* clang-format on */

int g_system = 1; /* the PS5's language id */
Language g_lang = Language::English;
std::unordered_map<std::string, std::string> g_map; /* English -> current language */
std::string g_empty;

const char *pick(const Entry &e, Language l)
{
    switch (l)
    {
    case Language::Spanish: return e.es;
    case Language::French: return e.fr;
    case Language::Portuguese: return e.pt;
    case Language::Italian: return e.it;
    case Language::Japanese: return e.ja;
    default: return e.en;
    }
}

Language from_ps5(int id)
{
    /* The PS5's ids, as the PS4's (and PS5SX2's table): 2/22 French,
     * 3/20 Spanish, 7/17 Portuguese, 5 Italian, 0 Japanese; everything else
     * English. */
    switch (id)
    {
    case 2:
    case 22: return Language::French;
    case 3:
    case 20: return Language::Spanish;
    case 7:
    case 17: return Language::Portuguese;
    case 5: return Language::Italian;
    case 0: return Language::Japanese;
    default: return Language::English;
    }
}

const char *code(Language l)
{
    switch (l)
    {
    case Language::Spanish: return "es";
    case Language::French: return "fr";
    case Language::Portuguese: return "pt";
    case Language::Italian: return "it";
    case Language::Japanese: return "ja";
    default: return "en";
    }
}
} // namespace

void set_system_language(int ps5_language)
{
    g_system = ps5_language;
}

void apply_language(int setting, const std::string &override_dir)
{
    switch (setting)
    {
    case 1: g_lang = Language::English; break;
    case 2: g_lang = Language::Spanish; break;
    case 3: g_lang = Language::French; break;
    case 4: g_lang = Language::Portuguese; break;
    case 5: g_lang = Language::Italian; break;
    case 6: g_lang = Language::Japanese; break;
    default: g_lang = from_ps5(g_system); break;
    }
    g_map.clear();
    if (g_lang != Language::English)
        for (const Entry &e : kTable)
            g_map[e.en] = pick(e, g_lang);
    /* A tester's corrections: "English = translation" lines. */
    if (!override_dir.empty() && g_lang != Language::English)
        if (std::FILE *f = std::fopen((override_dir + "/" + code(g_lang) + ".txt").c_str(), "r"))
        {
            char line[1024];
            while (std::fgets(line, sizeof line, f))
            {
                std::string s = line;
                while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
                    s.pop_back();
                const auto eq = s.find(" = ");
                if (s.empty() || s[0] == '#' || eq == std::string::npos)
                    continue;
                g_map[s.substr(0, eq)] = s.substr(eq + 3);
            }
            std::fclose(f);
        }
}

Language language()
{
    return g_lang;
}

const char *language_choice(int setting)
{
    static std::string system;
    switch (setting)
    {
    case 1: return "English";
    case 2: return "Espa\xC3\xB1ol";
    case 3: return "Fran\xC3\xA7" "ais";
    case 4: return "Portugu\xC3\xAAs";
    case 5: return "Italiano";
    case 6: return "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";
    default:
        system = kSystemChoice[int(g_lang)];
        return system.c_str();
    }
}

const std::string &tr(const std::string &english)
{
    if (g_lang == Language::English)
        return english;
    const auto it = g_map.find(english);
    return it == g_map.end() ? english : it->second;
}

const std::string &trc(const char *context, const std::string &english)
{
    if (g_lang == Language::English)
        return english;
    const auto it = g_map.find(std::string(context) + "|" + english);
    return it == g_map.end() ? tr(english) : it->second;
}

std::string trf(const std::string &english, std::initializer_list<std::pair<const char *, std::string>> values)
{
    std::string s = tr(english);
    for (const auto &v : values)
    {
        const std::string key = std::string("{") + v.first + "}";
        for (std::size_t at = s.find(key); at != std::string::npos; at = s.find(key, at + v.second.size()))
            s.replace(at, key.size(), v.second);
    }
    return s;
}

std::string plural(long long n, const char *one, const char *many)
{
    if (n == 1)
        return tr(one);
    return trf(many, {{"n", std::to_string(n)}});
}
} // namespace porpoise::ui
