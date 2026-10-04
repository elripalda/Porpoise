/* Porpoise UI - the menus in the player's language.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * First translations, to be read over by native speakers: corrections go in
 * /data/porpoise/lang/es.txt (fr.txt, pt.txt) as "English = translation"
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
    const char *en, *es, *fr, *pt;
};

/* clang-format off */
const Entry kTable[] = {
    /* Top bar, tabs, library */
    {"Library", "Biblioteca", "Bibliothèque", "Biblioteca"},
    {"Memory Cards", "Tarjetas de memoria", "Cartes mémoire", "Cartões de memória"},
    {"Settings", "Ajustes", "Paramètres", "Definições"},
    {"Your games", "Tus juegos", "Tes jeux", "Os teus jogos"},
    {"1 game", "1 juego", "1 jeu", "1 jogo"},
    {"{n} games", "{n} juegos", "{n} jeux", "{n} jogos"},
    {"No games yet", "Aún no hay juegos", "Pas encore de jeux", "Ainda sem jogos"},
    {"Add games", "Añade juegos", "Ajoute des jeux", "Adiciona jogos"},
    {"Copy .iso, .rvz or .ciso files with PS5 Upload into", "Copia archivos .iso, .rvz o .ciso con PS5 Upload en",
     "Copie des fichiers .iso, .rvz ou .ciso avec PS5 Upload dans", "Copia ficheiros .iso, .rvz ou .ciso com o PS5 Upload para"},
    {"or add your own folder in Settings, under Games.", "o añade tu propia carpeta en Ajustes, en Juegos.",
     "ou ajoute ton propre dossier dans Paramètres, sous Jeux.", "ou adiciona a tua própria pasta em Definições, em Jogos."},
    {"Play", "Jugar", "Jouer", "Jogar"},
    {"Browse", "Explorar", "Parcourir", "Navegar"},
    {"Details", "Detalles", "Détails", "Detalhes"},
    {"Sort", "Ordenar", "Trier", "Ordenar"},
    {"Sort games", "Ordenar juegos", "Trier les jeux", "Ordenar jogos"},
    {"Title A-Z", "Título A-Z", "Titre A-Z", "Título A-Z"},
    {"Recently played", "Jugados recientemente", "Joués récemment", "Jogados recentemente"},
    {"Current", "Actual", "Actuel", "Atual"},
    {"Back", "Atrás", "Retour", "Voltar"},
    {"Choose", "Elegir", "Choisir", "Escolher"},
    {"Confirm", "Confirmar", "Confirmer", "Confirmar"},
    {"Cancel", "Cancelar", "Annuler", "Cancelar"},
    {"OK", "Aceptar", "OK", "OK"},
    {"Select", "Seleccionar", "Sélectionner", "Selecionar"},

    /* Play history */
    {"Not played yet", "Aún sin jugar", "Pas encore joué", "Ainda não jogado"},
    {"Played just now", "Jugado hace un momento", "Joué à l'instant", "Jogado agora mesmo"},
    {"Played today", "Jugado hoy", "Joué aujourd'hui", "Jogado hoje"},
    {"Last played yesterday", "Jugado por última vez ayer", "Joué pour la dernière fois hier", "Jogado pela última vez ontem"},
    {"Last played {n} days ago", "Jugado hace {n} días", "Joué il y a {n} jours", "Jogado há {n} dias"},
    {"Last played {n} weeks ago", "Jugado hace {n} semanas", "Joué il y a {n} semaines", "Jogado há {n} semanas"},
    {"Last played a while ago", "Jugado hace tiempo", "Joué il y a longtemps", "Jogado há algum tempo"},

    /* Regions */
    {"USA", "EE. UU.", "États-Unis", "EUA"},
    {"Japan", "Japón", "Japon", "Japão"},
    {"Korea", "Corea", "Corée", "Coreia"},
    {"Europe", "Europa", "Europe", "Europa"},
    {"Germany", "Alemania", "Allemagne", "Alemanha"},
    {"France", "Francia", "France", "França"},
    {"Spain", "España", "Espagne", "Espanha"},
    {"Italy", "Italia", "Italie", "Itália"},
    {"Australia", "Australia", "Australie", "Austrália"},

    /* Details */
    {"Developer", "Desarrollador", "Développeur", "Produtora"},
    {"Publisher", "Editor", "Éditeur", "Editora"},
    {"Released", "Lanzamiento", "Sortie", "Lançamento"},
    {"Genre", "Género", "Genre", "Género"},
    {"Players", "Jugadores", "Joueurs", "Jogadores"},
    {"Rating", "Clasificación", "Classification", "Classificação"},
    {"Last played", "Última partida", "Dernière partie", "Última vez"},
    {"File", "Archivo", "Fichier", "Ficheiro"},
    {"No description yet. It arrives with the game info from GameTDB.com.",
     "Aún no hay descripción. Llegará con la información del juego de GameTDB.com.",
     "Pas encore de description. Elle arrivera avec les infos du jeu de GameTDB.com.",
     "Ainda sem descrição. Chega com a informação do jogo do GameTDB.com."},
    {"Turn on Settings > Games > Download game info for a description.",
     "Activa Ajustes > Juegos > Descargar información para ver una descripción.",
     "Active Paramètres > Jeux > Télécharger les infos pour avoir une description.",
     "Ativa Definições > Jogos > Transferir informação para ver uma descrição."},
    {"Game settings", "Ajustes del juego", "Paramètres du jeu", "Definições do jogo"},
    {"Back of box", "Reverso de la caja", "Dos de la boîte", "Verso da caixa"},
    {"Front of box", "Frente de la caja", "Face de la boîte", "Frente da caixa"},
    {"Right stick: turn the box", "Stick derecho: girar la caja", "Stick droit : tourner la boîte",
     "Analógico direito: rodar a caixa"},
    {"Save data", "Datos guardados", "Sauvegardes", "Dados guardados"},
    {"Custom", "Personalizado", "Personnalisé", "Personalizado"},

    /* Memory cards */
    {"Slot {slot}", "Ranura {slot}", "Port {slot}", "Ranhura {slot}"},
    {"SLOT {slot}", "RANURA {slot}", "PORT {slot}", "RANHURA {slot}"},
    {"memcard|Open", "Libres", "Libres", "Livres"},
    {"Open", "Abrir", "Ouvrir", "Abrir"},
    {"No saves yet", "Sin partidas guardadas", "Pas de sauvegarde", "Sem jogos guardados"},
    {"1 save", "1 partida", "1 sauvegarde", "1 jogo guardado"},
    {"{n} saves", "{n} partidas", "{n} sauvegardes", "{n} jogos guardados"},
    {"1 block", "1 bloque", "1 bloc", "1 bloco"},
    {"{n} blocks", "{n} bloques", "{n} blocs", "{n} blocos"},
    {"Nothing saved in Slot {slot} yet", "Aún no hay nada en la ranura {slot}", "Rien dans le port {slot} pour l'instant",
     "Ainda nada guardado na ranhura {slot}"},
    {"Your saves appear here after you save in a game.", "Tus partidas aparecen aquí cuando guardas en un juego.",
     "Tes sauvegardes apparaissent ici quand tu sauvegardes dans un jeu.",
     "Os teus jogos guardados aparecem aqui quando guardas num jogo."},
    {"Copy to {slot}", "Copiar a {slot}", "Copier vers {slot}", "Copiar para {slot}"},
    {"Copy", "Copiar", "Copier", "Copiar"},
    {"Delete", "Borrar", "Supprimer", "Apagar"},
    {"Replace", "Reemplazar", "Remplacer", "Substituir"},
    {"Delete this save?", "¿Borrar esta partida?", "Supprimer cette sauvegarde ?", "Apagar este jogo guardado?"},
    {"{save}. It is removed from Slot {slot} for good.", "{save}. Se borrará de la ranura {slot} para siempre.",
     "{save}. Elle sera supprimée du port {slot} pour de bon.", "{save}. É apagado da ranhura {slot} para sempre."},
    {"Copy to Slot {slot}?", "¿Copiar a la ranura {slot}?", "Copier vers le port {slot} ?", "Copiar para a ranhura {slot}?"},
    {"{save} ({blocks}) is copied to Slot {slot}.", "{save} ({blocks}) se copiará a la ranura {slot}.",
     "{save} ({blocks}) sera copiée vers le port {slot}.", "{save} ({blocks}) é copiado para a ranhura {slot}."},
    {"Replace the save on Slot {slot}?", "¿Reemplazar la partida de la ranura {slot}?",
     "Remplacer la sauvegarde du port {slot} ?", "Substituir o jogo guardado na ranhura {slot}?"},
    {"Slot {slot} already has {save}. Copying replaces it with this one.",
     "La ranura {slot} ya tiene {save}. Al copiar se reemplaza por esta.",
     "Le port {slot} contient déjà {save}. La copie la remplace par celle-ci.",
     "A ranhura {slot} já tem {save}. Copiar substitui-o por este."},
    {"Not enough room", "No hay espacio suficiente", "Pas assez de place", "Sem espaço suficiente"},
    {"Slot {slot} has {free} blocks open; this save needs {need}.",
     "La ranura {slot} tiene {free} bloques libres; esta partida necesita {need}.",
     "Le port {slot} a {free} blocs libres ; cette sauvegarde en demande {need}.",
     "A ranhura {slot} tem {free} blocos livres; este jogo guardado precisa de {need}."},
    {"This save is inside a card file", "Esta partida está dentro de un archivo de tarjeta",
     "Cette sauvegarde est dans un fichier de carte", "Este jogo guardado está dentro de um ficheiro de cartão"},
    {"Porpoise can delete saves that Dolphin keeps as files (its GCI folders), not ones inside a .raw memory card image.",
     "Porpoise puede borrar partidas que Dolphin guarda como archivos (sus carpetas GCI), no las de una imagen .raw de tarjeta.",
     "Porpoise peut supprimer les sauvegardes que Dolphin garde en fichiers (ses dossiers GCI), pas celles d'une image .raw.",
     "O Porpoise apaga jogos que o Dolphin guarda como ficheiros (pastas GCI), não os de uma imagem .raw de cartão."},
    {"This save can't be copied", "Esta partida no se puede copiar", "Cette sauvegarde ne peut pas être copiée",
     "Este jogo guardado não pode ser copiado"},
    {"Porpoise copies saves that Dolphin keeps as files (its GCI folders).",
     "Porpoise copia partidas que Dolphin guarda como archivos (sus carpetas GCI).",
     "Porpoise copie les sauvegardes que Dolphin garde en fichiers (ses dossiers GCI).",
     "O Porpoise copia jogos que o Dolphin guarda como ficheiros (as pastas GCI)."},
    {"Could not delete the save", "No se pudo borrar la partida", "Impossible de supprimer la sauvegarde",
     "Não foi possível apagar o jogo guardado"},
    {"The file could not be removed: {path}", "No se pudo borrar el archivo: {path}", "Le fichier n'a pas pu être supprimé : {path}",
     "Não foi possível remover o ficheiro: {path}"},
    {"Could not copy the save", "No se pudo copiar la partida", "Impossible de copier la sauvegarde",
     "Não foi possível copiar o jogo guardado"},
    {"Writing {path} failed.", "Falló al escribir {path}.", "L'écriture de {path} a échoué.", "Falhou a escrita de {path}."},

    /* Launch */
    {"Launching", "Iniciando", "Lancement", "A iniciar"},
    {"Starting {game}\xE2\x80\xA6", "Iniciando {game}\xE2\x80\xA6", "Lancement de {game}\xE2\x80\xA6", "A iniciar {game}\xE2\x80\xA6"},
    {"Starting game\xE2\x80\xA6", "Iniciando juego\xE2\x80\xA6", "Lancement du jeu\xE2\x80\xA6", "A iniciar o jogo\xE2\x80\xA6"},
    {"Loading Dolphin", "Cargando Dolphin", "Chargement de Dolphin", "A carregar o Dolphin"},
    {"Reading the disc", "Leyendo el disco", "Lecture du disque", "A ler o disco"},
    {"Preparing graphics", "Preparando los gráficos", "Préparation des graphismes", "A preparar os gráficos"},
    {"This game didn't start", "Este juego no arrancó", "Ce jeu n'a pas démarré", "Este jogo não arrancou"},
    {"Dolphin could not start it. The file may be damaged or in a format Porpoise can't read yet. Details are in porpoise/core.log.",
     "Dolphin no pudo iniciarlo. El archivo puede estar dañado o en un formato que Porpoise aún no lee. Detalles en porpoise/core.log.",
     "Dolphin n'a pas pu le lancer. Le fichier est peut-être abîmé ou dans un format que Porpoise ne lit pas encore. Détails dans porpoise/core.log.",
     "O Dolphin não o conseguiu iniciar. O ficheiro pode estar danificado ou num formato que o Porpoise ainda não lê. Detalhes em porpoise/core.log."},

    /* In-game menu */
    {"PAUSED", "EN PAUSA", "EN PAUSE", "EM PAUSA"},
    {"Resume", "Continuar", "Reprendre", "Continuar"},
    {"FPS counter", "Contador de FPS", "Compteur FPS", "Contador de FPS"},
    {"Upscaling", "Escalado", "Mise à l'échelle", "Escala"},
    {"Volume", "Volumen", "Volume", "Volume"},
    {"Quit to library", "Salir a la biblioteca", "Retour à la bibliothèque", "Sair para a biblioteca"},
    {"Close Porpoise", "Cerrar Porpoise", "Fermer Porpoise", "Fechar o Porpoise"},
    {"Experimental: may slow some games down.", "Experimental: puede ralentizar algunos juegos.",
     "Expérimental : peut ralentir certains jeux.", "Experimental: pode tornar alguns jogos mais lentos."},
    {"Changes here are saved for this game.", "Los cambios se guardan para este juego.",
     "Les changements sont gardés pour ce jeu.", "As alterações ficam guardadas para este jogo."},
    {"4x (1440p) \xE2\x80\xA2 exp.", "4x (1440p) \xE2\x80\xA2 exp.", "4x (1440p) \xE2\x80\xA2 exp.", "4x (1440p) \xE2\x80\xA2 exp."},

    /* Settings: sections */
    {"Games", "Juegos", "Jeux", "Jogos"},
    {"Video", "Vídeo", "Vidéo", "Vídeo"},
    {"Graphics", "Gráficos", "Graphismes", "Gráficos"},
    {"Audio", "Audio", "Audio", "Áudio"},
    {"Controls", "Controles", "Commandes", "Controlos"},
    {"System", "Sistema", "Système", "Sistema"},
    {"Interface", "Interfaz", "Interface", "Interface"},
    {"About", "Acerca de", "À propos", "Sobre"},
    {"This game", "Este juego", "Ce jeu", "Este jogo"},
    {"GAME SETTINGS", "AJUSTES DEL JUEGO", "PARAMÈTRES DU JEU", "DEFINIÇÕES DO JOGO"},
    {"Sections", "Secciones", "Sections", "Secções"},
    {"Change", "Cambiar", "Modifier", "Alterar"},
    {"Search", "Buscar", "Rechercher", "Procurar"},
    {"Choose a folder", "Elegir carpeta", "Choisir un dossier", "Escolher pasta"},
    {"Remove", "Quitar", "Retirer", "Remover"},
    {"Reset", "Restablecer", "Réinitialiser", "Repor"},
    {"Reset\xE2\x80\xA6", "Restablecer\xE2\x80\xA6", "Réinitialiser\xE2\x80\xA6", "Repor\xE2\x80\xA6"},
    {"Choose\xE2\x80\xA6", "Elegir\xE2\x80\xA6", "Choisir\xE2\x80\xA6", "Escolher\xE2\x80\xA6"},
    {"On", "Sí", "Oui", "Sim"},
    {"Off", "No", "Non", "Não"},
    {"Changes apply the next time a game starts", "Los cambios se aplican la próxima vez que inicies un juego",
     "Les changements s'appliquent au prochain lancement d'un jeu", "As alterações aplicam-se na próxima vez que um jogo iniciar"},
    {"Where Porpoise looks for games, and what it downloads for them", "Dónde busca Porpoise los juegos y qué descarga para ellos",
     "Où Porpoise cherche les jeux, et ce qu'il télécharge pour eux", "Onde o Porpoise procura jogos e o que transfere para eles"},
    {"How Porpoise looks and reads", "Cómo se ve y se lee Porpoise", "L'apparence et la langue de Porpoise", "O aspeto e a língua do Porpoise"},
    {"Values in blue are this game's own", "Los valores en azul son propios de este juego", "Les valeurs en bleu sont propres à ce jeu",
     "Os valores a azul são deste jogo"},
    {"For this game only \xE2\x80\xA2 values in blue are its own", "Solo para este juego \xE2\x80\xA2 los valores en azul son suyos",
     "Pour ce jeu seulement \xE2\x80\xA2 les valeurs en bleu sont les siennes", "Só para este jogo \xE2\x80\xA2 os valores a azul são dele"},
    {"Choose a section with up and down, then press Right or Cross to go into it.",
     "Elige una sección con arriba y abajo, y pulsa Derecha o X para entrar.",
     "Choisis une section avec haut et bas, puis appuie sur Droite ou Croix pour y entrer.",
     "Escolhe uma secção com cima e baixo e prime Direita ou X para entrar."},

    /* Settings: Games */
    {"Find games automatically", "Buscar juegos automáticamente", "Trouver les jeux automatiquement", "Encontrar jogos automaticamente"},
    {"Looks in /data/porpoise/games, /data/games, /data/roms, /data/iso and on USB drives.",
     "Busca en /data/porpoise/games, /data/games, /data/roms, /data/iso y en unidades USB.",
     "Cherche dans /data/porpoise/games, /data/games, /data/roms, /data/iso et sur les clés USB.",
     "Procura em /data/porpoise/games, /data/games, /data/roms, /data/iso e em unidades USB."},
    {"Download covers", "Descargar portadas", "Télécharger les jaquettes", "Transferir capas"},
    {"Box art from GameTDB.com, saved in /data/porpoise/covers. Needs the console online.",
     "Portadas de GameTDB.com, guardadas en /data/porpoise/covers. La consola debe estar en línea.",
     "Jaquettes de GameTDB.com, gardées dans /data/porpoise/covers. La console doit être en ligne.",
     "Capas do GameTDB.com, guardadas em /data/porpoise/covers. A consola tem de estar online."},
    {"Download game info", "Descargar información de juegos", "Télécharger les infos des jeux", "Transferir informação dos jogos"},
    {"Descriptions, developers, release dates and disc art from GameTDB.com, for Details.",
     "Descripciones, desarrolladores, fechas y arte del disco de GameTDB.com, para Detalles.",
     "Descriptions, développeurs, dates de sortie et images du disque de GameTDB.com, pour Détails.",
     "Descrições, produtoras, datas e arte do disco do GameTDB.com, para Detalhes."},
    {"Porpoise looks in this folder and four levels below it. Cross stops looking here; files stay.",
     "Porpoise busca en esta carpeta y cuatro niveles más abajo. X deja de buscar aquí; los archivos se quedan.",
     "Porpoise cherche dans ce dossier et quatre niveaux en dessous. Croix arrête de chercher ici ; les fichiers restent.",
     "O Porpoise procura nesta pasta e em quatro níveis abaixo. X deixa de procurar aqui; os ficheiros ficam."},
    {"Add a game folder", "Añadir carpeta de juegos", "Ajouter un dossier de jeux", "Adicionar pasta de jogos"},
    {"Pick any folder on the console or a USB drive to search for games.",
     "Elige cualquier carpeta de la consola o de un USB para buscar juegos.",
     "Choisis n'importe quel dossier de la console ou d'une clé USB pour y chercher des jeux.",
     "Escolhe qualquer pasta da consola ou de uma unidade USB para procurar jogos."},
    {"Search for games now", "Buscar juegos ahora", "Chercher les jeux maintenant", "Procurar jogos agora"},
    {"Looks through every folder again, for games you have just copied over.",
     "Vuelve a revisar todas las carpetas, por si acabas de copiar juegos.",
     "Repasse tous les dossiers, pour les jeux que tu viens de copier.",
     "Volta a ver todas as pastas, para jogos que acabaste de copiar."},

    /* Settings: Video */
    {"Internal resolution", "Resolución interna", "Résolution interne", "Resolução interna"},
    {"How sharp games render. 1080p is the tested default; above it is experimental and can slow games.",
     "Qué tan nítidos se ven los juegos. 1080p es lo probado; por encima es experimental y puede ralentizar.",
     "La netteté des jeux. 1080p est la valeur testée ; au-delà c'est expérimental et ça peut ralentir.",
     "A nitidez dos jogos. 1080p é o valor testado; acima disso é experimental e pode tornar lento."},
    {"4x (1440p) \xE2\x80\xA2 experimental", "4x (1440p) \xE2\x80\xA2 experimental", "4x (1440p) \xE2\x80\xA2 expérimental",
     "4x (1440p) \xE2\x80\xA2 experimental"},
    {"5x (1800p) \xE2\x80\xA2 experimental", "5x (1800p) \xE2\x80\xA2 experimental", "5x (1800p) \xE2\x80\xA2 expérimental",
     "5x (1800p) \xE2\x80\xA2 experimental"},
    {"6x (4K) \xE2\x80\xA2 experimental", "6x (4K) \xE2\x80\xA2 experimental", "6x (4K) \xE2\x80\xA2 expérimental",
     "6x (4K) \xE2\x80\xA2 experimental"},
    {"Widescreen hack", "Hack de pantalla ancha", "Hack écran large", "Hack de ecrã panorâmico"},
    {"Draws games in 16:9. Some games show glitches at the screen edges.",
     "Muestra los juegos en 16:9. Algunos tienen fallos en los bordes.",
     "Affiche les jeux en 16:9. Certains montrent des défauts sur les bords.",
     "Mostra os jogos em 16:9. Alguns têm falhas nas margens."},
    {"Aspect ratio", "Relación de aspecto", "Format d'image", "Proporção"},
    {"The picture's shape. Auto follows the game; Stretch fills the screen.",
     "La forma de la imagen. Auto sigue al juego; Estirar llena la pantalla.",
     "La forme de l'image. Auto suit le jeu ; Étirer remplit l'écran.",
     "A forma da imagem. Auto segue o jogo; Esticar preenche o ecrã."},
    {"Auto", "Auto", "Auto", "Auto"},
    {"Force 16:9", "Forzar 16:9", "Forcer 16:9", "Forçar 16:9"},
    {"Force 4:3", "Forzar 4:3", "Forcer 4:3", "Forçar 4:3"},
    {"Stretch to fill", "Estirar", "Étirer", "Esticar"},
    {"Anti-aliasing", "Antialiasing", "Anticrénelage", "Anti-aliasing"},
    {"Smooths jagged edges. SSAA is the sharpest and the heaviest.",
     "Suaviza los bordes dentados. SSAA es el más nítido y el más pesado.",
     "Adoucit les bords crénelés. Le SSAA est le plus net et le plus lourd.",
     "Suaviza os contornos serrilhados. O SSAA é o mais nítido e o mais pesado."},
    {"Anisotropic filtering", "Filtrado anisotrópico", "Filtrage anisotrope", "Filtragem anisotrópica"},
    {"Sharper textures on floors and walls seen at an angle.",
     "Texturas más nítidas en suelos y paredes vistos en ángulo.",
     "Textures plus nettes sur les sols et les murs vus de biais.",
     "Texturas mais nítidas em chãos e paredes vistos de lado."},
    {"Texture filtering", "Filtrado de texturas", "Filtrage des textures", "Filtragem de texturas"},
    {"Force sharp or smooth textures, or leave it to the game.",
     "Fuerza texturas nítidas o suaves, o deja que decida el juego.",
     "Force des textures nettes ou lisses, ou laisse le jeu décider.",
     "Força texturas nítidas ou suaves, ou deixa o jogo decidir."},
    {"Game's own", "El del juego", "Celui du jeu", "O do jogo"},
    {"Nearest (sharp)", "Vecino (nítido)", "Plus proche (net)", "Vizinho (nítido)"},
    {"Linear (smooth)", "Lineal (suave)", "Linéaire (lisse)", "Linear (suave)"},
    {"Output resampling", "Remuestreo de salida", "Rééchantillonnage", "Reamostragem de saída"},
    {"How Dolphin scales its picture. Sharp bilinear keeps pixels crisp.",
     "Cómo escala Dolphin la imagen. Bilineal nítido mantiene los píxeles definidos.",
     "Comment Dolphin met l'image à l'échelle. Bilinéaire net garde les pixels précis.",
     "Como o Dolphin escala a imagem. Bilinear nítido mantém os píxeis definidos."},
    {"Default", "Predeterminado", "Par défaut", "Predefinido"},
    {"Sharp bilinear", "Bilineal nítido", "Bilinéaire net", "Bilinear nítido"},
    {"Area sampling", "Muestreo por área", "Échantillonnage par zone", "Amostragem por área"},
    {"Upscaling to the TV", "Escalado a la TV", "Mise à l'échelle sur la TV", "Escala para a TV"},
    {"How Porpoise fits the picture to your TV.", "Cómo ajusta Porpoise la imagen a tu TV.",
     "Comment Porpoise adapte l'image à ta TV.", "Como o Porpoise ajusta a imagem à tua TV."},
    {"Smooth", "Suave", "Lisse", "Suave"},
    {"Sharp", "Nítido", "Net", "Nítido"},
    {"FPS overlay", "Mostrar FPS", "Afficher les FPS", "Mostrar FPS"},
    {"Shows the frame rate in the corner while you play.", "Muestra los fotogramas por segundo en una esquina mientras juegas.",
     "Affiche les images par seconde dans un coin pendant que tu joues.", "Mostra as imagens por segundo num canto enquanto jogas."},

    /* Settings: Graphics */
    {"Shader compilation", "Compilación de shaders", "Compilation des shaders", "Compilação de shaders"},
    {"Ubershaders hide the stutter when a game draws something new, at a GPU cost.",
     "Los ubershaders evitan tirones cuando el juego dibuja algo nuevo, a costa de la GPU.",
     "Les ubershaders évitent les saccades quand le jeu affiche du nouveau, au prix du GPU.",
     "Os ubershaders evitam soluços quando o jogo desenha algo novo, à custa da GPU."},
    {"Synchronous", "Síncrono", "Synchrone", "Síncrono"},
    {"Ubershaders", "Ubershaders", "Ubershaders", "Ubershaders"},
    {"Async ubershaders", "Ubershaders asíncronos", "Ubershaders asynchrones", "Ubershaders assíncronos"},
    {"Async, skip drawing", "Asíncrono, sin dibujar", "Asynchrone, sans dessin", "Assíncrono, sem desenhar"},
    {"Texture cache accuracy", "Precisión de la caché de texturas", "Précision du cache de textures", "Precisão da cache de texturas"},
    {"Safe fixes some games' text and effects; Fast is quickest.",
     "Seguro arregla texto y efectos de algunos juegos; Rápido es lo más veloz.",
     "Sûr corrige le texte et les effets de certains jeux ; Rapide est le plus vif.",
     "Seguro corrige texto e efeitos de alguns jogos; Rápido é o mais veloz."},
    {"Fast", "Rápido", "Rapide", "Rápido"},
    {"Middle", "Medio", "Moyen", "Médio"},
    {"Safe", "Seguro", "Sûr", "Seguro"},
    {"Per-pixel lighting", "Iluminación por píxel", "Éclairage par pixel", "Iluminação por píxel"},
    {"Smoother lighting on surfaces. A little heavier.", "Iluminación más suave en superficies. Algo más pesada.",
     "Un éclairage plus doux sur les surfaces. Un peu plus lourd.", "Iluminação mais suave nas superfícies. Um pouco mais pesada."},
    {"Disable fog", "Desactivar niebla", "Désactiver le brouillard", "Desativar nevoeiro"},
    {"Removes distance fog. Some games use fog for their look.",
     "Quita la niebla lejana. Algunos juegos la usan para su estilo.",
     "Retire le brouillard au loin. Certains jeux l'utilisent pour leur ambiance.",
     "Remove o nevoeiro ao longe. Alguns jogos usam-no para o seu visual."},
    {"Crop overscan", "Recortar bordes", "Rogner les bords", "Cortar margens"},
    {"Hides the black borders some games draw at the edges.", "Oculta los bordes negros que dibujan algunos juegos.",
     "Masque les bandes noires que certains jeux dessinent sur les bords.", "Esconde as margens pretas que alguns jogos desenham."},
    {"Custom textures", "Texturas personalizadas", "Textures personnalisées", "Texturas personalizadas"},
    {"Loads texture packs from /data/porpoise/saves/User/Load/Textures/<game ID>.",
     "Carga packs de texturas de /data/porpoise/saves/User/Load/Textures/<ID del juego>.",
     "Charge les packs de textures depuis /data/porpoise/saves/User/Load/Textures/<ID du jeu>.",
     "Carrega packs de texturas de /data/porpoise/saves/User/Load/Textures/<ID do jogo>."},
    {"Skip duplicate frames", "Omitir fotogramas duplicados", "Ignorer les images en double", "Saltar imagens repetidas"},
    {"Saves work when a game shows the same frame twice.", "Ahorra trabajo cuando un juego muestra el mismo fotograma dos veces.",
     "Évite du travail quand un jeu affiche deux fois la même image.", "Poupa trabalho quando um jogo mostra a mesma imagem duas vezes."},

    /* Settings: Audio, Controls */
    {"Menu music", "Música del menú", "Musique du menu", "Música do menu"},
    {"The music that plays in Porpoise's menus.", "La música que suena en los menús de Porpoise.",
     "La musique qui joue dans les menus de Porpoise.", "A música que toca nos menus do Porpoise."},
    {"Music volume", "Volumen de la música", "Volume de la musique", "Volume da música"},
    {"How loud the menu music plays.", "Qué tan fuerte suena la música del menú.", "Le volume de la musique du menu.",
     "O volume da música do menu."},
    {"Menu sounds", "Sonidos del menú", "Sons du menu", "Sons do menu"},
    {"The sounds of moving through the menus.", "Los sonidos al moverte por los menús.",
     "Les sons quand tu te déplaces dans les menus.", "Os sons ao navegar pelos menus."},
    {"Sounds volume", "Volumen de los sonidos", "Volume des sons", "Volume dos sons"},
    {"How loud the menu sounds play.", "Qué tan fuerte suenan los sonidos del menú.", "Le volume des sons du menu.",
     "O volume dos sons do menu."},
    {"Game volume", "Volumen del juego", "Volume du jeu", "Volume do jogo"},
    {"Volume of the game's sound.", "Volumen del sonido del juego.", "Le volume du son du jeu.", "O volume do som do jogo."},
    {"Mute game", "Silenciar juego", "Couper le son du jeu", "Silenciar jogo"},
    {"Silences the game.", "Silencia el juego.", "Coupe le son du jeu.", "Silencia o jogo."},
    {"Button layout", "Distribución de botones", "Disposition des boutons", "Disposição dos botões"},
    {"GameCube: Cross is A, like confirming on PlayStation. PlayStation: by position.",
     "GameCube: X es A, como confirmar en PlayStation. PlayStation: por posición.",
     "GameCube : Croix est A, comme pour valider sur PlayStation. PlayStation : par position.",
     "GameCube: X é A, como confirmar na PlayStation. PlayStation: pela posição."},
    {"PlayStation", "PlayStation", "PlayStation", "PlayStation"},
    {"GameCube", "GameCube", "GameCube", "GameCube"},
    {"Vibration", "Vibración", "Vibration", "Vibração"},
    {"Controller rumble.", "Vibración del mando.", "Les vibrations de la manette.", "A vibração do comando."},

    /* Settings: System */
    {"CPU clock", "Reloj de CPU", "Fréquence du CPU", "Relógio do CPU"},
    {"Overclocking can smooth a game that slows down. 100% is the real console.",
     "Subirlo puede suavizar un juego que se ralentiza. 100% es la consola real.",
     "L'augmenter peut fluidifier un jeu qui ralentit. 100 % est la vraie console.",
     "Aumentá-lo pode suavizar um jogo que fica lento. 100% é a consola real."},
    {"Dual core", "Doble núcleo", "Double cœur", "Dois núcleos"},
    {"Faster. Turn it off for a game that freezes or glitches.", "Más rápido. Desactívalo si un juego se congela o falla.",
     "Plus rapide. Désactive-le pour un jeu qui gèle ou bugue.", "Mais rápido. Desativa-o num jogo que congela ou falha."},
    {"Fast disc loading", "Carga rápida del disco", "Chargement rapide du disque", "Carregamento rápido do disco"},
    {"Shorter loading screens. A few games need real disc speed.",
     "Pantallas de carga más cortas. Algunos juegos necesitan la velocidad real.",
     "Des écrans de chargement plus courts. Quelques jeux exigent la vitesse réelle.",
     "Ecrãs de carregamento mais curtos. Alguns jogos precisam da velocidade real."},
    {"Cheats", "Trucos", "Codes de triche", "Batotas"},
    {"Dolphin's cheat codes for games that have them.", "Los trucos de Dolphin para los juegos que los tienen.",
     "Les codes de triche de Dolphin pour les jeux qui en ont.", "As batotas do Dolphin para os jogos que as têm."},
    {"System language", "Idioma de la consola", "Langue de la console", "Idioma da consola"},
    {"The console's language. European games show their text in it.",
     "El idioma de la consola. Los juegos europeos muestran su texto en él.",
     "La langue de la console. Les jeux européens affichent leur texte dans celle-ci.",
     "O idioma da consola. Os jogos europeus mostram o texto nele."},
    {"English", "Inglés", "Anglais", "Inglês"},
    {"Japanese", "Japonés", "Japonais", "Japonês"},
    {"German", "Alemán", "Allemand", "Alemão"},
    {"French", "Francés", "Français", "Francês"},
    {"Spanish", "Español", "Espagnol", "Espanhol"},
    {"Italian", "Italiano", "Italien", "Italiano"},
    {"Dutch", "Neerlandés", "Néerlandais", "Neerlandês"},
    {"Chinese (simplified)", "Chino (simplificado)", "Chinois (simplifié)", "Chinês (simplificado)"},
    {"Chinese (traditional)", "Chino (tradicional)", "Chinois (traditionnel)", "Chinês (tradicional)"},
    {"Korean", "Coreano", "Coréen", "Coreano"},
    {"Progressive scan", "Escaneo progresivo", "Balayage progressif", "Varrimento progressivo"},
    {"480p output, as on a component cable.", "Salida 480p, como con cable de componentes.",
     "Sortie 480p, comme avec un câble composante.", "Saída 480p, como com cabo de componentes."},

    /* Settings: Interface */
    {"Language", "Idioma", "Langue", "Idioma"},
    {"The language of Porpoise's menus. System follows your PS5.", "El idioma de los menús de Porpoise. Sistema sigue a tu PS5.",
     "La langue des menus de Porpoise. Système suit ta PS5.", "O idioma dos menus do Porpoise. Sistema segue a tua PS5."},
    {"Reduced motion", "Menos movimiento", "Mouvements réduits", "Menos movimento"},
    {"Stops the moving lights and shortens animations.", "Detiene las luces en movimiento y acorta las animaciones.",
     "Arrête les lumières mobiles et raccourcit les animations.", "Para as luzes em movimento e encurta as animações."},
    {"Larger text", "Texto más grande", "Texte plus grand", "Texto maior"},
    {"Bigger labels across Porpoise.", "Textos más grandes en todo Porpoise.", "Des textes plus grands dans Porpoise.",
     "Textos maiores em todo o Porpoise."},
    {"Reset all settings", "Restablecer todos los ajustes", "Réinitialiser tous les paramètres", "Repor todas as definições"},
    {"Every setting back to how Porpoise ships. Games, folders and saves stay.",
     "Todos los ajustes vuelven a como vienen. Juegos, carpetas y partidas se quedan.",
     "Tous les paramètres reviennent à l'origine. Jeux, dossiers et sauvegardes restent.",
     "Todas as definições voltam ao original. Jogos, pastas e jogos guardados ficam."},
    {"Reset all settings?", "¿Restablecer todos los ajustes?", "Réinitialiser tous les paramètres ?", "Repor todas as definições?"},
    {"Every setting goes back to how Porpoise ships. Your games, folders, covers and saves stay.",
     "Todos los ajustes vuelven a como vienen. Tus juegos, carpetas, portadas y partidas se quedan.",
     "Tous les paramètres reviennent à l'origine. Tes jeux, dossiers, jaquettes et sauvegardes restent.",
     "Todas as definições voltam ao original. Os teus jogos, pastas, capas e jogos guardados ficam."},

    /* Settings: a game's own */
    {"Own settings", "Ajustes propios", "Paramètres propres", "Definições próprias"},
    {"Changes here apply to this game only. Everything else follows your settings.",
     "Los cambios aquí son solo para este juego. Lo demás sigue tus ajustes.",
     "Les changements ici ne valent que pour ce jeu. Le reste suit tes paramètres.",
     "As alterações aqui só valem para este jogo. O resto segue as tuas definições."},
    {"None yet", "Ninguno aún", "Aucun pour l'instant", "Nenhuma ainda"},
    {"1 change", "1 cambio", "1 changement", "1 alteração"},
    {"{n} changes", "{n} cambios", "{n} changements", "{n} alterações"},
    {"Reset to default", "Restablecer", "Revenir par défaut", "Repor predefinição"},
    {"Forgets this game's own settings; it follows your settings again.",
     "Olvida los ajustes propios de este juego; vuelve a seguir tus ajustes.",
     "Oublie les paramètres propres à ce jeu ; il suit à nouveau les tiens.",
     "Esquece as definições deste jogo; volta a seguir as tuas."},
    {"Reset this game's settings?", "¿Restablecer los ajustes de este juego?", "Réinitialiser les paramètres de ce jeu ?",
     "Repor as definições deste jogo?"},
    {"It forgets its own settings and follows your settings again.", "Olvida sus ajustes propios y vuelve a seguir los tuyos.",
     "Il oublie ses paramètres propres et suit à nouveau les tiens.", "Esquece as suas definições e volta a seguir as tuas."},

    /* Folder browser */
    {"Choose a game folder", "Elige una carpeta de juegos", "Choisis un dossier de jeux", "Escolhe uma pasta de jogos"},
    {"Pick a drive", "Elige una unidad", "Choisis un lecteur", "Escolhe uma unidade"},
    {"Console storage", "Almacenamiento de la consola", "Stockage de la console", "Armazenamento da consola"},
    {"USB drive {n}", "Unidad USB {n}", "Clé USB {n}", "Unidade USB {n}"},
    {"Extended storage", "Almacenamiento ampliado", "Stockage étendu", "Armazenamento expandido"},
    {"Whole system", "Todo el sistema", "Tout le système", "Todo o sistema"},
    {"No games right here", "No hay juegos justo aquí", "Pas de jeu ici même", "Sem jogos mesmo aqui"},
    {"1 game right here", "1 juego aquí", "1 jeu ici", "1 jogo aqui"},
    {"{n} games right here", "{n} juegos aquí", "{n} jeux ici", "{n} jogos aqui"},
    {"No folders inside this one", "No hay carpetas dentro", "Aucun dossier à l'intérieur", "Sem pastas dentro desta"},
    {"Use this folder", "Usar esta carpeta", "Utiliser ce dossier", "Usar esta pasta"},
    {"Up", "Subir", "Remonter", "Subir"},

    /* About */
    {"Porpoise for PS5", "Porpoise para PS5", "Porpoise pour PS5", "Porpoise para PS5"},
    {"created by @elripalda", "creado por @elripalda", "créé par @elripalda", "criado por @elripalda"},
    {"Created by", "Creado por", "Créé par", "Criado por"},
    {"Website", "Sitio web", "Site web", "Site"},
    {"Emulation", "Emulación", "Émulation", "Emulação"},
    {"Dolphin core", "Núcleo de Dolphin", "Cœur Dolphin", "Núcleo do Dolphin"},
    {"Core API", "API del núcleo", "API du cœur", "API do núcleo"},
    {"PS5 graphics", "Gráficos de PS5", "Graphismes PS5", "Gráficos da PS5"},
    {"PS5 toolchain", "Herramientas PS5", "Outils PS5", "Ferramentas PS5"},
    {"PS5 port", "Port a PS5", "Portage PS5", "Port para PS5"},
    {"Inspired by", "Inspirado en", "Inspiré par", "Inspirado em"},
    {"Box art and info", "Portadas e información", "Jaquettes et infos", "Capas e informação"},
    {"Font", "Fuente", "Police", "Tipo de letra"},
    {"Images and audio", "Imágenes y audio", "Images et audio", "Imagens e áudio"},
    {"Music and sounds", "Música y sonidos", "Musique et sons", "Música e sons"},
    {"The menu music and sound effects, made for Porpoise by Ruben.", "La música y los efectos del menú, hechos para Porpoise por Ruben.",
     "La musique et les effets du menu, créés pour Porpoise par Ruben.", "A música e os efeitos do menu, feitos para o Porpoise pelo Ruben."},
    {"Thanks", "Gracias", "Merci", "Agradecimentos"},
    {"Trademarks", "Marcas", "Marques", "Marcas"},
    {"A GameCube and Wii player for PS5, in the spirit of the GameCube's own menus.",
     "Un reproductor de GameCube y Wii para PS5, con el espíritu de los menús de GameCube.",
     "Un lecteur GameCube et Wii pour PS5, dans l'esprit des menus de la GameCube.",
     "Um leitor de GameCube e Wii para a PS5, no espírito dos menus da GameCube."},
    {"Updates, news and more from the creator of Porpoise.", "Novedades y más del creador de Porpoise.",
     "Les nouveautés et plus encore du créateur de Porpoise.", "Novidades e mais do criador do Porpoise."},
    {"PS5 homebrew front ends that showed the way.", "Interfaces homebrew de PS5 que marcaron el camino.",
     "Des interfaces homebrew PS5 qui ont montré la voie.", "Interfaces homebrew da PS5 que abriram caminho."},
    {"etaHEN, kstuff and ShadowMountPlus make homebrew like this possible.",
     "etaHEN, kstuff y ShadowMountPlus hacen posible el homebrew como este.",
     "etaHEN, kstuff et ShadowMountPlus rendent possible ce genre de homebrew.",
     "O etaHEN, o kstuff e o ShadowMountPlus tornam possível homebrew como este."},
    {"GameCube and Wii are trademarks of Nintendo. Porpoise is not affiliated with Nintendo.",
     "GameCube y Wii son marcas de Nintendo. Porpoise no está afiliado a Nintendo.",
     "GameCube et Wii sont des marques de Nintendo. Porpoise n'est pas affilié à Nintendo.",
     "GameCube e Wii são marcas da Nintendo. O Porpoise não é afiliado da Nintendo."},
    {"Covers, disc art and game details from GameTDB.com and its contributors.",
     "Portadas, arte de discos y detalles de GameTDB.com y sus colaboradores.",
     "Jaquettes, images de disque et détails de GameTDB.com et de ses contributeurs.",
     "Capas, arte de discos e detalhes do GameTDB.com e dos seus colaboradores."},
    {"Porpoise hosts the core through the libretro API that RetroArch made.",
     "Porpoise ejecuta el núcleo con la API libretro creada por RetroArch.",
     "Porpoise fait tourner le cœur via l'API libretro créée par RetroArch.",
     "O Porpoise corre o núcleo com a API libretro criada pelo RetroArch."},
    {"The PS5 Dolphin and RetroArch port work Porpoise is built on.",
     "El trabajo de port de Dolphin y RetroArch a PS5 sobre el que se construye Porpoise.",
     "Le travail de portage de Dolphin et RetroArch sur PS5 sur lequel repose Porpoise.",
     "O trabalho de port do Dolphin e do RetroArch para a PS5 em que o Porpoise assenta."},

    /* Covers status (main) */
    {"Getting game info", "Descargando información", "Récupération des infos", "A obter informação"},
    {"Getting covers {done} of {total}", "Descargando portadas {done} de {total}", "Jaquettes {done} sur {total}",
     "A obter capas {done} de {total}"},
    {"Getting disc art {done} of {total}", "Descargando discos {done} de {total}", "Images de disque {done} sur {total}",
     "A obter discos {done} de {total}"},
};

const char *const kSystemChoice[] = {"System", "Sistema", "Système", "Sistema"};
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
    default: return e.en;
    }
}

Language from_ps5(int id)
{
    /* The PS5's ids, as the PS4's (and PS5SX2's table): 2/22 French,
     * 3/20 Spanish, 7/17 Portuguese; everything else English. */
    switch (id)
    {
    case 2:
    case 22: return Language::French;
    case 3:
    case 20: return Language::Spanish;
    case 7:
    case 17: return Language::Portuguese;
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
