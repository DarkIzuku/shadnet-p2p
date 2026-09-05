// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
"use strict";

(() => {
  const translations = {
    en: {
      menu: "Menu", skipToContent: "Skip to content", home: "Home", hunters: "Hunters", stats: "Stats", register: "Register",
      login: "Login", account: "Account", loading: "Loading", unavailable: "The dream is quiet",
      tryAgain: "Try again", unofficial: "Unofficial community project. Not affiliated with Sony or FromSoftware.",
      footerQuote: "“May the night guide every wandering hunter.”", eyebrow: "A gathering beyond the veil",
      subtitle: "An unofficial community server for wandering hunters.", createAccount: "Create Account",
      enterDream: "Enter the Dream", huntersOnline: "Hunters Online", registeredHunters: "Registered Hunters",
      coopSessions: "Co-op Sessions", messages: "Messages", nowInDream: "now in the dream",
      joinedHunt: "joined the hunt", activeRooms: "active rooms", storedEchoes: "stored echoes",
      recentActivity: "Recent Activity", seeAll: "See all", noActivity: "No recent activity.",
      onlineHunters: "Online Hunters", noHuntersOnline: "No visible hunters are online.",
      online: "Online", offline: "Offline", registered: "Registered", lastSeen: "Last seen",
      sessionDuration: "session", playersLead: "Seek those who have answered the call.",
      searchHunters: "Search hunters", search: "Search", noPlayers: "No hunters found.",
      profile: "Hunter Profile", overview: "Overview", totalSessions: "Total sessions",
      totalOnlineTime: "Total online time", community: "Community", messagesCreated: "Messages created",
      bloodstainsCreated: "Bloodstains created", ghostsGenerated: "Ghosts generated",
      multiplayer: "Multiplayer", activeMatchingRooms: "Active matching rooms",
      summonsAdvertised: "Summons advertised", summonClaims: "Summon claims",
      successfulCoopPending: "Successful co-op history", notMeasured: "Not measured yet",
      registerTitle: "Join the Hunt", registerLead: "Create a normal shadNet account for the game and this community.",
      username: "Username", password: "Password", confirmPassword: "Confirm password",
      showPassword: "Show password", hidePassword: "Hide password",
      create: "Create Account", registrationClosed: "Registration is currently closed.",
      accountCreated: "Account created. You can now enter the dream.", alreadyAccount: "Already registered?",
      loginTitle: "Enter the Dream", loginLead: "Use the same shadNet account you use in the game.",
      signIn: "Sign In", newHunter: "New hunter?", invalidCredentials: "Invalid username or password.",
      accountTitle: "Your Account", changeAvatar: "Change avatar", avatarHelp: "PNG, JPEG or WebP · 2 MB maximum · stored as a safe 512×512 PNG.",
      uploadAvatar: "Upload Avatar", logout: "Log Out", avatarUpdated: "Avatar updated.",
      chooseImage: "Choose an image first.", sessionExpired: "Your web session has expired.",
      connected: "connected", disconnected: "disconnected", leftMessage: "left a message",
      leftBloodstain: "left a bloodstain", ghostAppeared: "left a wandering ghost",
      summonAdvertised: "rang a beckoning bell", summonClaimed: "answered a summon",
      secondsAgo: "just now", minuteAgo: "1 minute ago", minutesAgo: "{n} minutes ago",
      hourAgo: "1 hour ago", hoursAgo: "{n} hours ago", dayAgo: "1 day ago", daysAgo: "{n} days ago",
      unknown: "Unknown", requestFailed: "The server could not complete the request.",
      exactMetrics: "Live values derived directly from authenticated clients, accounts, Matching2 rooms and stored messages.",
      communion: "Communion", communionTitle: "Hunter's Communion",
      communionLead: "Share words with every hunter gathered beyond the veil.",
      huntersListening: "{n} hunters online", noChatMessages: "The communion is silent.",
      chatPlaceholder: "Write a message...", send: "Send", loginToChat: "Enter the dream to speak.",
      chatReset: "The communion is cleansed every {n} hours.", chatTooFast: "Wait a moment before speaking again.",
      chalices: "Chalices", chalicesTitle: "Chalice Dungeons",
      chalicesLead: "Explore locally hosted dungeons and copy their glyphs into Bloodborne.",
      storedChalices: "Stored Chalices", exactGlyph: "Exact Glyph", glyph: "Glyph",
      depth: "Depth / Ritual Level", allDepths: "All depths", typeId: "Holy Grail Type ID",
      ritesId: "SubFeatureFlag", filters: "Apply filters", clear: "Clear",
      noChalices: "No open Chalices match these filters.", creator: "Creator",
      imported: "Imported", privacy: "Privacy", open: "Open", closed: "Closed",
      unshared: "Unshared", copyGlyph: "Copy Glyph", copied: "Copied", previous: "Previous",
      next: "Next", pageOf: "Page {page} of {pages}", chaliceDetails: "Chalice Details",
      fixedOrGeneral: "FixedOrGeneral", status: "Status", subFeatureFlag: "SubFeatureFlag",
      channelId: "ChannelId", created: "Created", lastPlayed: "Last played",
      formDataVersion: "FormDataVersion", formDataBytes: "FormData size", bytes: "bytes",
      dungeonMap: "Dungeon Map", mapPending: "Map data not yet decoded",
      backToChalices: "Back to Chalices", downloads: "Downloads",
      downloadsTitle: "Downloads", downloadsLead: "Official files shared by this server's administrators.",
      availableDownloads: "Available Downloads", noDownloads: "No downloads are available yet.",
      latestDownloads: "Latest Downloads", viewAllDownloads: "View all downloads",
      name: "Name", version: "Version", category: "Category", allCategories: "All categories",
      description: "Description", file: "File", fileName: "File name", size: "Size",
      sha256: "SHA-256", download: "Download", startingDownload: "Starting…", downloadCount: "Downloads", updated: "Updated",
      manageDownloads: "Manage Downloads", adminDownloadsTitle: "Manage Downloads",
      adminDownloadsLead: "Upload files and control what visitors can download.",
      active: "Active", inactive: "Inactive", uploadDownload: "Upload Download",
      edit: "Edit", replace: "Replace", disable: "Disable", enable: "Enable", delete: "Delete",
      save: "Save", cancel: "Cancel", chooseFile: "Choose a file first.",
      uploadComplete: "Download uploaded.", updateComplete: "Download updated.",
      replaceComplete: "File replaced.", deleteComplete: "Download deleted.",
      confirmDelete: "Delete this download and its stored file?", adminOnly: "Administrator access required."
    },
    es: {
      menu: "Menú", skipToContent: "Saltar al contenido", home: "Inicio", hunters: "Cazadores", stats: "Estadísticas", register: "Registro",
      login: "Entrar", account: "Cuenta", loading: "Cargando", unavailable: "El sueño guarda silencio",
      tryAgain: "Intentar de nuevo", unofficial: "Proyecto comunitario no oficial. Sin afiliación con Sony ni FromSoftware.",
      footerQuote: "“Que la noche guíe a cada cazador errante.”", eyebrow: "Un encuentro más allá del velo",
      subtitle: "Un servidor comunitario no oficial para cazadores errantes.", createAccount: "Crear cuenta",
      enterDream: "Entrar al sueño", huntersOnline: "Cazadores en línea", registeredHunters: "Cazadores registrados",
      coopSessions: "Sesiones co-op", messages: "Mensajes", nowInDream: "ahora en el sueño",
      joinedHunt: "se unieron a la caza", activeRooms: "salas activas", storedEchoes: "ecos guardados",
      recentActivity: "Actividad reciente", seeAll: "Ver todos", noActivity: "No hay actividad reciente.",
      onlineHunters: "Cazadores en línea", noHuntersOnline: "No hay cazadores visibles conectados.",
      online: "En línea", offline: "Desconectado", registered: "Registro", lastSeen: "Visto por última vez",
      sessionDuration: "sesión", playersLead: "Busca a quienes han respondido a la llamada.",
      searchHunters: "Buscar cazadores", search: "Buscar", noPlayers: "No se encontraron cazadores.",
      profile: "Perfil del cazador", overview: "Resumen", totalSessions: "Sesiones totales",
      totalOnlineTime: "Tiempo total en línea", community: "Comunidad", messagesCreated: "Mensajes creados",
      bloodstainsCreated: "Manchas de sangre", ghostsGenerated: "Espectros generados",
      multiplayer: "Multijugador", activeMatchingRooms: "Salas de matching activas",
      summonsAdvertised: "Invocaciones publicadas", summonClaims: "Invocaciones reclamadas",
      successfulCoopPending: "Historial de co-op exitoso", notMeasured: "Aún no medido",
      registerTitle: "Únete a la caza", registerLead: "Crea una cuenta shadNet normal para el juego y esta comunidad.",
      username: "Usuario", password: "Contraseña", confirmPassword: "Confirmar contraseña",
      showPassword: "Mostrar contraseña", hidePassword: "Ocultar contraseña",
      create: "Crear cuenta", registrationClosed: "El registro está cerrado actualmente.",
      accountCreated: "Cuenta creada. Ya puedes entrar al sueño.", alreadyAccount: "¿Ya tienes cuenta?",
      loginTitle: "Entra al sueño", loginLead: "Usa la misma cuenta shadNet que utilizas en el juego.",
      signIn: "Iniciar sesión", newHunter: "¿Nuevo cazador?", invalidCredentials: "Usuario o contraseña incorrectos.",
      accountTitle: "Tu cuenta", changeAvatar: "Cambiar avatar", avatarHelp: "PNG, JPEG o WebP · máximo 2 MB · se guarda como PNG seguro de hasta 512×512.",
      uploadAvatar: "Subir avatar", logout: "Cerrar sesión", avatarUpdated: "Avatar actualizado.",
      chooseImage: "Primero elige una imagen.", sessionExpired: "Tu sesión web ha caducado.",
      connected: "se conectó", disconnected: "se desconectó", leftMessage: "dejó un mensaje",
      leftBloodstain: "dejó una mancha de sangre", ghostAppeared: "dejó un espectro errante",
      summonAdvertised: "hizo sonar una campana de convocación", summonClaimed: "respondió a una invocación",
      secondsAgo: "ahora mismo", minuteAgo: "hace 1 minuto", minutesAgo: "hace {n} minutos",
      hourAgo: "hace 1 hora", hoursAgo: "hace {n} horas", dayAgo: "hace 1 día", daysAgo: "hace {n} días",
      unknown: "Desconocido", requestFailed: "El servidor no pudo completar la solicitud.",
      exactMetrics: "Valores en vivo obtenidos directamente de clientes autenticados, cuentas, salas Matching2 y mensajes almacenados.",
      communion: "Comunión", communionTitle: "Comunión de Cazadores",
      communionLead: "Comparte palabras con todos los cazadores reunidos más allá del velo.",
      huntersListening: "{n} cazadores en línea", noChatMessages: "La comunión guarda silencio.",
      chatPlaceholder: "Escribe un mensaje...", send: "Enviar", loginToChat: "Entra al sueño para hablar.",
      chatReset: "La comunión se purifica cada {n} horas.", chatTooFast: "Espera un momento antes de volver a hablar.",
      chalices: "Cálices", chalicesTitle: "Mazmorras de Cáliz",
      chalicesLead: "Explora mazmorras alojadas localmente y copia sus glyphs en Bloodborne.",
      storedChalices: "Cálices almacenados", exactGlyph: "Glyph exacto", glyph: "Glyph",
      depth: "Profundidad / Ritual Level", allDepths: "Todas las profundidades", typeId: "Holy Grail Type ID",
      ritesId: "SubFeatureFlag", filters: "Aplicar filtros", clear: "Limpiar",
      noChalices: "Ningún Cáliz abierto coincide con estos filtros.", creator: "Creador",
      imported: "Importado", privacy: "Privacidad", open: "Abierto", closed: "Cerrado",
      unshared: "Sin compartir", copyGlyph: "Copiar Glyph", copied: "Copiado", previous: "Anterior",
      next: "Siguiente", pageOf: "Página {page} de {pages}", chaliceDetails: "Detalles del Cáliz",
      fixedOrGeneral: "FixedOrGeneral", status: "Status", subFeatureFlag: "SubFeatureFlag",
      channelId: "ChannelId", created: "Creado", lastPlayed: "Última partida",
      formDataVersion: "FormDataVersion", formDataBytes: "Tamaño de FormData", bytes: "bytes",
      dungeonMap: "Mapa de la mazmorra", mapPending: "Los datos del mapa aún no están decodificados",
      backToChalices: "Volver a Cálices", downloads: "Descargas",
      downloadsTitle: "Descargas", downloadsLead: "Archivos oficiales compartidos por los administradores de este servidor.",
      availableDownloads: "Descargas disponibles", noDownloads: "Todavía no hay descargas disponibles.",
      latestDownloads: "Últimas descargas", viewAllDownloads: "Ver todas las descargas",
      name: "Nombre", version: "Versión", category: "Categoría", allCategories: "Todas las categorías",
      description: "Descripción", file: "Archivo", fileName: "Nombre de archivo", size: "Tamaño",
      sha256: "SHA-256", download: "Descargar", startingDownload: "Iniciando…", downloadCount: "Descargas", updated: "Actualizado",
      manageDownloads: "Gestionar descargas", adminDownloadsTitle: "Gestionar descargas",
      adminDownloadsLead: "Sube archivos y controla cuáles pueden descargar los visitantes.",
      active: "Activo", inactive: "Inactivo", uploadDownload: "Subir descarga",
      edit: "Editar", replace: "Reemplazar", disable: "Desactivar", enable: "Activar", delete: "Borrar",
      save: "Guardar", cancel: "Cancelar", chooseFile: "Primero elige un archivo.",
      uploadComplete: "Descarga subida.", updateComplete: "Descarga actualizada.",
      replaceComplete: "Archivo reemplazado.", deleteComplete: "Descarga borrada.",
      confirmDelete: "¿Borrar esta descarga y su archivo guardado?", adminOnly: "Se requiere acceso de administrador."
    }
  };

  const state = {
    language: localStorage.getItem("thr-language") === "en" ? "en" : "es",
    account: null,
    websiteStatus: null,
    chatEnabled: false,
    chatPollTimer: null,
    chatPollGeneration: 0,
    revealObserver: null
  };
  const app = document.getElementById("app");
  const t = (key, values = {}) => {
    let text = translations[state.language][key] || translations.en[key] || key;
    Object.entries(values).forEach(([name, value]) => { text = text.replace(`{${name}}`, String(value)); });
    return text;
  };

  function node(tag, className, text) {
    const element = document.createElement(tag);
    if (className) element.className = className;
    if (text !== undefined) element.textContent = text;
    return element;
  }

  async function api(path, options = {}) {
    const response = await fetch(path, {
      credentials: "same-origin",
      headers: { Accept: "application/json", ...(options.headers || {}) },
      ...options
    });
    let payload = null;
    try { payload = await response.json(); } catch (_) { payload = null; }
    if (!response.ok || !payload || payload.ok !== true) {
      const error = new Error(payload?.error?.message || t("requestFailed"));
      error.code = payload?.error?.code || "request_failed";
      error.status = response.status;
      throw error;
    }
    return payload.data;
  }

  function applyLanguage() {
    document.documentElement.lang = state.language;
    document.querySelectorAll("[data-i18n]").forEach((element) => {
      element.textContent = t(element.dataset.i18n);
    });
    document.querySelectorAll("[data-language]").forEach((button) => {
      button.classList.toggle("active", button.dataset.language === state.language);
      button.setAttribute("aria-pressed", button.dataset.language === state.language ? "true" : "false");
    });
  }

  function formatNumber(value) {
    return new Intl.NumberFormat(state.language === "es" ? "es-ES" : "en-US").format(Number(value || 0));
  }

  function formatBytes(value) {
    let bytes = Math.max(0, Number(value || 0));
    const units = ["B", "KiB", "MiB", "GiB"];
    let unit = 0;
    while (bytes >= 1024 && unit < units.length - 1) { bytes /= 1024; unit += 1; }
    return `${new Intl.NumberFormat(state.language === "es" ? "es-ES" : "en-US", {
      maximumFractionDigits: unit === 0 ? 0 : 1
    }).format(bytes)} ${units[unit]}`;
  }

  function formatDuration(seconds) {
    const total = Math.max(0, Number(seconds || 0));
    const days = Math.floor(total / 86400);
    const hours = Math.floor((total % 86400) / 3600);
    const minutes = Math.floor((total % 3600) / 60);
    if (days) return `${days}d ${hours}h`;
    if (hours) return `${hours}h ${minutes}m`;
    return `${minutes}m`;
  }

  function relativeTime(timestamp) {
    if (!timestamp) return t("unknown");
    const seconds = Math.max(0, Math.floor(Date.now() / 1000 - Number(timestamp)));
    if (seconds < 60) return t("secondsAgo");
    const minutes = Math.floor(seconds / 60);
    if (minutes === 1) return t("minuteAgo");
    if (minutes < 60) return t("minutesAgo", { n: minutes });
    const hours = Math.floor(minutes / 60);
    if (hours === 1) return t("hourAgo");
    if (hours < 24) return t("hoursAgo", { n: hours });
    const days = Math.floor(hours / 24);
    return days === 1 ? t("dayAgo") : t("daysAgo", { n: days });
  }

  function dateText(timestamp) {
    if (!timestamp) return t("unknown");
    return new Intl.DateTimeFormat(state.language === "es" ? "es-ES" : "en-US", {
      year: "numeric", month: "long", day: "numeric"
    }).format(new Date(Number(timestamp) * 1000));
  }

  function localTime(isoTimestamp) {
    const date = new Date(isoTimestamp);
    if (Number.isNaN(date.getTime())) return "";
    return new Intl.DateTimeFormat(state.language === "es" ? "es-ES" : "en-US", {
      hour: "2-digit", minute: "2-digit"
    }).format(date);
  }

  function chaliceDate(isoTimestamp) {
    if (!isoTimestamp) return t("unknown");
    const normalized = /(?:Z|[+-]\d\d:\d\d)$/.test(isoTimestamp) ? isoTimestamp : `${isoTimestamp}Z`;
    const date = new Date(normalized);
    if (Number.isNaN(date.getTime())) return isoTimestamp;
    return new Intl.DateTimeFormat(state.language === "es" ? "es-ES" : "en-US", {
      year: "numeric", month: "short", day: "numeric", hour: "2-digit", minute: "2-digit"
    }).format(date);
  }

  function avatar(player, large = false) {
    const wrapper = node("span", `avatar${large ? " large" : ""}`);
    const initial = Array.from(player.username || "?")[0]?.toUpperCase() || "?";
    wrapper.textContent = initial;
    if (player.avatarUrl && player.avatarUrl.startsWith("/avatars/")) {
      const image = document.createElement("img");
      image.src = player.avatarUrl;
      image.alt = "";
      image.addEventListener("load", () => { wrapper.textContent = ""; wrapper.append(image); }, { once: true });
    }
    return wrapper;
  }

  function statusText(player) {
    return player.online
      ? `${t("online")} · ${formatDuration(player.currentSessionSeconds)}`
      : `${t("lastSeen")} ${relativeTime(player.lastSeen)}`;
  }

  function playerRow(player, card = false) {
    const link = node("a", card ? "player-card" : "list-row");
    link.href = `/player/${encodeURIComponent(player.username)}`;
    link.append(avatar(player));
    const main = node("span", "list-main");
    main.append(node("span", "list-title", player.username));
    const meta = node("span", "list-meta");
    const dot = node("span", `status-dot${player.online ? " online" : ""}`);
    meta.append(dot, document.createTextNode(statusText(player)));
    main.append(meta);
    link.append(main);
    return link;
  }

  function creatorBlock(creator) {
    if (!creator || creator.kind !== "local" || !creator.username) {
      return node("span", "chalice-creator imported", t("imported"));
    }
    const link = node("a", "chalice-creator");
    link.href = creator.profileUrl || `/player/${encodeURIComponent(creator.username)}`;
    link.append(avatar(creator), node("span", "creator-name", creator.username));
    return link;
  }

  function privacyText(level) {
    return t(Number(level) === 2 ? "open" : Number(level) === 1 ? "closed" : "unshared");
  }

  async function copyGlyph(glyph, button) {
    try {
      await navigator.clipboard.writeText(glyph);
    } catch (_) {
      const input = document.createElement("textarea");
      input.value = glyph;
      input.setAttribute("readonly", "");
      input.style.position = "fixed";
      input.style.opacity = "0";
      document.body.append(input);
      input.select();
      document.execCommand("copy");
      input.remove();
    }
    if (button) {
      const original = button.textContent;
      button.textContent = t("copied");
      button.classList.add("is-copied");
      button.setAttribute("aria-live", "polite");
      setTimeout(() => {
        if (!button.isConnected) return;
        button.textContent = original;
        button.classList.remove("is-copied");
        button.removeAttribute("aria-live");
      }, 1200);
    }
  }

  function glyphButton(glyph, compact = false) {
    const button = node("button", `button quiet glyph-copy${compact ? " compact" : ""}`, t("copyGlyph"));
    button.type = "button";
    button.addEventListener("click", (event) => {
      event.preventDefault();
      event.stopPropagation();
      copyGlyph(glyph, button);
    });
    return button;
  }

  function setPage(markupClass = "page inner-page") {
    state.revealObserver?.disconnect();
    state.revealObserver = null;
    app.replaceChildren();
    const page = node("section", markupClass);
    app.append(page);
    return page;
  }

  function titleBlock(page, titleKey, leadKey, eyebrowKey = "eyebrow") {
    page.append(node("p", "eyebrow", t(eyebrowKey)));
    page.append(node("h1", "page-title", t(titleKey)));
    page.append(node("p", "page-lead", t(leadKey)));
  }

  function downloadCard(download, compact = false) {
    const card = node("article", `download-card${compact ? " compact" : ""}`);
    const heading = node("div", "download-heading");
    const identity = node("div", "download-identity");
    identity.append(node("h2", "download-name", download.displayName));
    const badges = node("div", "download-badges");
    badges.append(node("span", "download-category", download.category));
    if (download.version) badges.append(node("span", "download-version", `v${download.version}`));
    identity.append(badges);
    const action = node("a", "button primary download-button", t("download"));
    action.href = download.downloadUrl;
    action.setAttribute("download", download.originalFilename || "");
    action.addEventListener("click", () => {
      const original = action.textContent;
      action.textContent = t("startingDownload");
      action.classList.add("is-downloading");
      action.setAttribute("aria-busy", "true");
      setTimeout(() => {
        if (!action.isConnected) return;
        action.textContent = original;
        action.classList.remove("is-downloading");
        action.removeAttribute("aria-busy");
      }, 1800);
    });
    heading.append(identity, action);
    card.append(heading);
    if (download.description) card.append(node("p", "download-description", download.description));

    const details = node("dl", "download-details");
    const item = (label, value, className = "") => {
      const wrapper = node("div", `download-detail ${className}`.trim());
      wrapper.append(node("dt", "", t(label)), node("dd", "", value));
      details.append(wrapper);
    };
    item("fileName", download.originalFilename);
    item("size", formatBytes(download.fileSize));
    item("updated", dateText(download.updatedAt));
    item("downloadCount", formatNumber(download.downloadCount));
    if (!compact) item("sha256", download.sha256, "download-hash");
    card.append(details);
    return card;
  }

  function showError(error) {
    const page = setPage();
    page.append(node("h1", "page-title", t("unavailable")));
    const panel = node("div", "error-panel", error?.message || t("requestFailed"));
    page.append(panel);
    const retry = node("button", "button quiet", t("tryAgain"));
    retry.type = "button";
    retry.addEventListener("click", renderRoute);
    page.append(retry);
  }

  function activityText(item) {
    const labels = {
      player_connected: "connected", player_disconnected: "disconnected",
      message_created: "leftMessage", bloodstain_created: "leftBloodstain",
      ghost_created: "ghostAppeared", summon_advertised: "summonAdvertised",
      summon_claimed: "summonClaimed"
    };
    return t(labels[item.type] || "unknown");
  }

  async function renderHome() {
    const page = setPage("page home-page");
    const hero = node("section", "hero");
    const emblem = document.createElement("img");
    emblem.className = "hero-emblem";
    emblem.src = "/assets/requiem-emblem.png";
    emblem.alt = "";
    hero.append(emblem, node("p", "eyebrow", t("eyebrow")));
    const heading = node("h1");
    heading.append(document.createTextNode("The Hunter's"), document.createElement("br"), document.createTextNode("Requiem"));
    hero.append(heading, node("p", "hero-subtitle", t("subtitle")));
    const actions = node("div", "hero-actions");
    const register = node("a", "button primary", t("createAccount")); register.href = "/register";
    const enter = node("a", "button", t("enterDream")); enter.href = state.account ? "/account" : "/login";
    actions.append(register, enter); hero.append(actions); page.append(hero);

    const [status, playersData, activityData, downloadsData] = await Promise.all([
      api("/api/status"), api("/api/players?online=true&limit=6"), api("/api/activity?limit=6"),
      api("/api/downloads?limit=3")
    ]);
    state.websiteStatus = status;
    state.chatEnabled = status.chatEnabled === true;
    updateChatNav();
    const strip = node("section", "stats-strip"); strip.id = "stats";
    const stats = [
      ["huntersOnline", status.huntersOnline, "nowInDream"],
      ["registeredHunters", status.registeredHunters, "joinedHunt"],
      ["coopSessions", status.coOpSessions, "activeRooms"],
      ["messages", status.messages, "storedEchoes"]
    ];
    stats.forEach(([label, value, note]) => {
      const box = node("div", "stat");
      const icon = node("span", "stat-icon", "◇"); icon.setAttribute("aria-hidden", "true");
      box.append(icon, node("span", "stat-label", t(label)), node("strong", "stat-value", formatNumber(value)), node("span", "stat-note", t(note)));
      strip.append(box);
    });
    page.append(strip);

    const grid = node("section", "home-grid");
    const activityPanel = node("div", "panel");
    const activityHeader = node("div", "panel-header"); activityHeader.append(node("h2", "", t("recentActivity")));
    activityPanel.append(activityHeader);
    const activityList = node("ul", "list");
    if (!activityData.activity.length) activityList.append(node("li", "empty-state", t("noActivity")));
    activityData.activity.forEach((item) => {
      const row = node("li", "list-row");
      const main = node("span", "list-main");
      main.append(node("span", "list-title", item.username || t("unknown")), node("span", "list-meta", activityText(item)));
      row.append(main, node("time", "list-time", relativeTime(item.occurredAt))); activityList.append(row);
    });
    activityPanel.append(activityList);

    const onlinePanel = node("div", "panel");
    const onlineHeader = node("div", "panel-header"); onlineHeader.append(node("h2", "", t("onlineHunters")));
    const all = node("a", "", t("seeAll")); all.href = "/players"; onlineHeader.append(all); onlinePanel.append(onlineHeader);
    const onlineList = node("div", "list");
    if (!playersData.players.length) onlineList.append(node("div", "empty-state", t("noHuntersOnline")));
    playersData.players.forEach((player) => onlineList.append(playerRow(player)));
    onlinePanel.append(onlineList); grid.append(activityPanel, onlinePanel);
    if (downloadsData.downloads.length) {
      const downloadsPanel = node("section", "panel home-downloads");
      const downloadsHeader = node("div", "panel-header");
      downloadsHeader.append(node("h2", "", t("latestDownloads")));
      const allDownloads = node("a", "", t("viewAllDownloads"));
      allDownloads.href = "/downloads";
      downloadsHeader.append(allDownloads);
      const latest = node("div", "latest-download-grid");
      downloadsData.downloads.forEach((download) => latest.append(downloadCard(download, true)));
      downloadsPanel.append(downloadsHeader, latest);
      grid.append(downloadsPanel);
    }
    page.append(grid);
    const definition = node("p", "metrics-definition", t("exactMetrics")); page.append(definition);
  }

  async function renderPlayers() {
    const page = setPage(); titleBlock(page, "hunters", "playersLead");
    const form = node("form", "search-bar");
    const input = document.createElement("input"); input.type = "search"; input.name = "search"; input.maxLength = 64; input.placeholder = t("searchHunters");
    input.className = "search-input"; input.setAttribute("aria-label", t("searchHunters"));
    const submit = node("button", "button quiet", t("search")); submit.type = "submit"; form.append(input, submit); page.append(form);
    const grid = node("div", "players-grid"); page.append(grid);
    const load = async () => {
      grid.replaceChildren(node("div", "empty-state", t("loading")));
      const result = await api(`/api/players?limit=100&search=${encodeURIComponent(input.value.trim())}`);
      grid.replaceChildren();
      if (!result.players.length) grid.append(node("div", "empty-state", t("noPlayers")));
      result.players.forEach((player) => grid.append(playerRow(player, true)));
    };
    form.addEventListener("submit", (event) => { event.preventDefault(); load().catch(showError); });
    await load();
  }

  async function renderChalices() {
    const page = setPage("page inner-page chalice-page");
    titleBlock(page, "chalicesTitle", "chalicesLead");
    const current = new URLSearchParams(location.search);
    const form = node("form", "chalice-filters");

    const glyphField = node("label", "filter-field");
    glyphField.append(node("span", "", t("exactGlyph")));
    const glyph = document.createElement("input");
    glyph.type = "search";
    glyph.name = "glyph";
    glyph.maxLength = 8;
    glyph.pattern = "[2-9a-km-np-z]{4,8}";
    glyph.value = current.get("glyph") || "";
    glyph.placeholder = "n2vskrmr";
    glyphField.append(glyph);

    const depthField = node("label", "filter-field");
    depthField.append(node("span", "", t("depth")));
    const depth = document.createElement("select");
    depth.name = "depth";
    depth.append(new Option(t("allDepths"), ""));
    for (let value = 1; value <= 5; value += 1) depth.append(new Option(String(value), String(value)));
    depth.value = current.get("depth") || "";
    depthField.append(depth);

    const typeField = node("label", "filter-field");
    typeField.append(node("span", "", t("typeId")));
    const type = document.createElement("input");
    type.type = "number";
    type.name = "type";
    type.min = "0";
    type.value = current.get("type") || "";
    typeField.append(type);

    const ritesField = node("label", "filter-field");
    ritesField.append(node("span", "", t("ritesId")));
    const rites = document.createElement("input");
    rites.type = "number";
    rites.name = "rites";
    rites.min = "0";
    rites.value = current.get("rites") || "";
    ritesField.append(rites);

    const actions = node("div", "filter-actions");
    const apply = node("button", "button primary", t("filters"));
    apply.type = "submit";
    const clear = node("a", "button quiet", t("clear"));
    clear.href = "/chalice";
    actions.append(apply, clear);
    form.append(glyphField, depthField, typeField, ritesField, actions);
    page.append(form);
    form.addEventListener("submit", (event) => {
      event.preventDefault();
      const query = new URLSearchParams();
      new FormData(form).forEach((value, key) => {
        if (String(value).trim()) query.set(key, String(value).trim().toLowerCase());
      });
      history.pushState({}, "", `/chalice${query.size ? `?${query}` : ""}`);
      renderRoute();
    });

    current.set("limit", "24");
    const result = await api(`/api/chalices?${current}`);
    const summary = node("div", "chalice-summary");
    summary.append(node("span", "", t("storedChalices")),
      node("strong", "", formatNumber(result.storedTotal)));
    page.append(summary);

    const table = node("section", "chalice-table");
    const header = node("div", "chalice-row chalice-head");
    ["glyph", "depth", "creator", "privacy"].forEach((key) => header.append(node("span", "", t(key))));
    table.append(header);
    if (!result.chalices.length) table.append(node("div", "empty-state", t("noChalices")));
    result.chalices.forEach((chalice) => {
      const row = node("article", "chalice-row");
      const glyphCell = node("div", "chalice-glyph-cell");
      const link = node("a", "glyph-link", chalice.glyph);
      link.href = `/chalice/${encodeURIComponent(chalice.glyph)}`;
      glyphCell.append(link, glyphButton(chalice.glyph, true));
      row.append(glyphCell, node("span", "chalice-depth", String(chalice.ritualLevel)),
        creatorBlock(chalice.creator), node("span", "privacy-badge", privacyText(chalice.shareLevel)));
      table.append(row);
    });
    page.append(table);

    if (result.pages > 1) {
      const pagination = node("nav", "pagination");
      const pageLink = (label, target, disabled) => {
        const link = node("a", `button quiet${disabled ? " disabled" : ""}`, label);
        if (!disabled) {
          const query = new URLSearchParams(location.search);
          query.set("page", String(target));
          link.href = `/chalice?${query}`;
        }
        return link;
      };
      pagination.append(pageLink(t("previous"), result.page - 1, result.page <= 1),
        node("span", "", t("pageOf", { page: result.page, pages: result.pages })),
        pageLink(t("next"), result.page + 1, result.page >= result.pages));
      page.append(pagination);
    }
  }

  async function renderChaliceDetail(glyph) {
    const chalice = await api(`/api/chalices/${encodeURIComponent(glyph.toLowerCase())}`);
    const page = setPage("page inner-page chalice-detail-page");
    const back = node("a", "text-link chalice-back", `← ${t("backToChalices")}`);
    back.href = "/chalice";
    page.append(back);

    const hero = node("section", "chalice-hero");
    const identity = node("div");
    identity.append(node("p", "eyebrow", t("chaliceDetails")),
      node("h1", "chalice-glyph", chalice.glyph));
    const heroActions = node("div", "chalice-hero-actions");
    heroActions.append(glyphButton(chalice.glyph));
    hero.append(identity, heroActions);
    page.append(hero);

    const creatorSection = node("section", "detail-section chalice-creator-section");
    creatorSection.append(node("h2", "", t("creator")), creatorBlock(chalice.creator));
    page.append(creatorSection);

    const details = node("section", "detail-grid chalice-detail-grid");
    const identityPanel = node("article", "detail-section");
    identityPanel.append(node("h2", "", t("overview")), metricList([
      ["channelId", formatNumber(chalice.channelId)], ["depth", formatNumber(chalice.ritualLevel)],
      ["fixedOrGeneral", formatNumber(chalice.fixedOrGeneral)],
      ["typeId", formatNumber(chalice.holyGrailTypeId)]
    ]));
    const statePanel = node("article", "detail-section");
    statePanel.append(node("h2", "", t("status")), metricList([
      ["privacy", privacyText(chalice.shareLevel)], ["status", formatNumber(chalice.status)],
      ["subFeatureFlag", formatNumber(chalice.subFeatureFlag)],
      ["formDataVersion", formatNumber(chalice.formDataVersion)]
    ]));
    const datesPanel = node("article", "detail-section");
    datesPanel.append(node("h2", "", t("lastPlayed")), metricList([
      ["created", chaliceDate(chalice.createdAt)], ["lastPlayed", chaliceDate(chalice.lastPlayDate)],
      ["formDataBytes", `${formatNumber(chalice.formDataBytes)} ${t("bytes")}`]
    ]));
    details.append(identityPanel, statePanel, datesPanel);
    page.append(details);

    const map = node("section", "dungeon-map-placeholder");
    map.append(node("h2", "", t("dungeonMap")), node("div", "map-sigil", "◇"),
      node("p", "", t("mapPending")));
    page.append(map);
  }

  async function renderDownloads() {
    const page = setPage("page inner-page downloads-page");
    titleBlock(page, "downloadsTitle", "downloadsLead");
    const current = new URLSearchParams(location.search);
    current.set("limit", "24");
    const result = await api(`/api/downloads?${current}`);

    const toolbar = node("div", "downloads-toolbar");
    const categoryLabel = node("label", "filter-field");
    categoryLabel.append(node("span", "", t("category")));
    const category = document.createElement("select");
    category.append(new Option(t("allCategories"), ""));
    result.categories.forEach((value) => category.append(new Option(value, value)));
    category.value = new URLSearchParams(location.search).get("category") || "";
    category.addEventListener("change", () => {
      const query = new URLSearchParams();
      if (category.value) query.set("category", category.value);
      history.pushState({}, "", `/downloads${query.size ? `?${query}` : ""}`);
      renderRoute();
    });
    categoryLabel.append(category);
    const summary = node("div", "downloads-summary");
    summary.append(node("span", "", t("availableDownloads")),
      node("strong", "", formatNumber(result.total)));
    toolbar.append(categoryLabel, summary);
    page.append(toolbar);

    const grid = node("section", "downloads-grid");
    if (!result.downloads.length) grid.append(node("div", "empty-state", t("noDownloads")));
    result.downloads.forEach((download) => grid.append(downloadCard(download)));
    page.append(grid);

    if (result.pages > 1) {
      const pagination = node("nav", "pagination");
      const pageLink = (label, target, disabled) => {
        const link = node("a", `button quiet${disabled ? " disabled" : ""}`, label);
        if (!disabled) {
          const query = new URLSearchParams(location.search);
          query.set("page", String(target));
          link.href = `/downloads?${query}`;
        }
        return link;
      };
      pagination.append(pageLink(t("previous"), result.page - 1, result.page <= 1),
        node("span", "", t("pageOf", { page: result.page, pages: result.pages })),
        pageLink(t("next"), result.page + 1, result.page >= result.pages));
      page.append(pagination);
    }
  }

  function adminDownloadForm(categories, download = null, includeFile = false) {
    const form = node("form", "admin-download-form");
    const makeField = (labelKey, name, kind = "input") => {
      const wrapper = node("label", "field");
      wrapper.append(node("span", "", t(labelKey)));
      const input = document.createElement(kind);
      input.name = name;
      wrapper.append(input);
      return { wrapper, input };
    };
    const name = makeField("name", "displayName");
    name.input.required = true; name.input.maxLength = 120; name.input.value = download?.displayName || "";
    const version = makeField("version", "version");
    version.input.maxLength = 64; version.input.value = download?.version || "";
    const category = makeField("category", "category", "select");
    categories.forEach((value) => category.input.append(new Option(value, value)));
    category.input.value = download?.category || categories[0] || "Other";
    const description = makeField("description", "description", "textarea");
    description.input.maxLength = 4000; description.input.rows = 4;
    description.input.value = download?.description || "";
    form.append(name.wrapper, version.wrapper, category.wrapper, description.wrapper);

    let file = null;
    if (includeFile) {
      const fileField = makeField("file", "file");
      fileField.input.type = "file"; fileField.input.required = true;
      file = fileField.input;
      form.append(fileField.wrapper);
    }
    const activeLabel = node("label", "checkbox-field");
    const active = document.createElement("input");
    active.type = "checkbox"; active.name = "isActive";
    active.checked = download ? download.isActive === true : true;
    activeLabel.append(active, node("span", "", t("active")));
    form.append(activeLabel);
    return { form, name: name.input, version: version.input, category: category.input,
      description: description.input, active, file };
  }

  async function renderAdminDownloads() {
    if (!state.account?.isAdmin) throw Object.assign(new Error(t("adminOnly")), { status: 403 });
    const result = await api("/api/admin/downloads");
    const page = setPage("page inner-page admin-downloads-page");
    titleBlock(page, "adminDownloadsTitle", "adminDownloadsLead");

    const uploadPanel = node("section", "panel admin-upload-panel");
    const uploadHeader = node("div", "panel-header");
    uploadHeader.append(node("h2", "", t("uploadDownload")));
    const upload = adminDownloadForm(result.categories, null, true);
    const uploadButton = node("button", "button primary", t("uploadDownload"));
    uploadButton.type = "submit";
    const uploadMessage = node("p", "form-message");
    upload.form.append(uploadButton, uploadMessage);
    upload.form.addEventListener("submit", async (event) => {
      event.preventDefault();
      if (!upload.file?.files?.[0]) {
        uploadMessage.className = "form-message error"; uploadMessage.textContent = t("chooseFile"); return;
      }
      uploadButton.disabled = true; uploadMessage.textContent = "";
      const body = new FormData(upload.form);
      body.set("isActive", upload.active.checked ? "true" : "false");
      try {
        await api("/api/admin/downloads", { method: "POST",
          headers: { "X-CSRF-Token": state.account.csrfToken }, body });
        uploadMessage.className = "form-message success"; uploadMessage.textContent = t("uploadComplete");
        setTimeout(renderRoute, 250);
      } catch (error) {
        uploadMessage.className = "form-message error"; uploadMessage.textContent = error.message;
        uploadButton.disabled = false;
      }
    });
    uploadPanel.append(uploadHeader, upload.form);
    page.append(uploadPanel);

    const list = node("section", "admin-download-list");
    if (!result.downloads.length) list.append(node("div", "empty-state", t("noDownloads")));
    result.downloads.forEach((download) => {
      const card = node("article", `admin-download-row${download.isActive ? "" : " inactive"}`);
      const identity = node("div", "admin-download-identity");
      identity.append(node("h2", "download-name", download.displayName),
        node("p", "list-meta", `${download.category} · ${download.version || "—"} · ${formatBytes(download.fileSize)}`),
        node("code", "download-sha", download.sha256),
        node("span", "download-state", t(download.isActive ? "active" : "inactive")));
      const actions = node("div", "download-admin-actions");
      const editButton = node("button", "button quiet", t("edit")); editButton.type = "button";
      const replaceButton = node("button", "button quiet", t("replace")); replaceButton.type = "button";
      const toggleButton = node("button", "button quiet", t(download.isActive ? "disable" : "enable")); toggleButton.type = "button";
      const deleteButton = node("button", "button danger", t("delete")); deleteButton.type = "button";
      actions.append(editButton, replaceButton, toggleButton, deleteButton);
      card.append(identity, actions);

      const edit = adminDownloadForm(result.categories, download, false);
      edit.form.classList.add("inline-edit-form"); edit.form.hidden = true;
      const editActions = node("div", "download-admin-actions");
      const save = node("button", "button primary", t("save")); save.type = "submit";
      const cancel = node("button", "button quiet", t("cancel")); cancel.type = "button";
      const message = node("p", "form-message");
      const actionMessage = node("p", "form-message admin-action-message");
      editActions.append(save, cancel); edit.form.append(editActions, message); card.append(edit.form);
      card.append(actionMessage);
      editButton.addEventListener("click", () => { edit.form.hidden = !edit.form.hidden; });
      cancel.addEventListener("click", () => { edit.form.hidden = true; });
      edit.form.addEventListener("submit", async (event) => {
        event.preventDefault(); save.disabled = true;
        try {
          await api(`/api/admin/downloads/${download.id}`, { method: "PUT",
            headers: { "Content-Type": "application/json", "X-CSRF-Token": state.account.csrfToken },
            body: JSON.stringify({ displayName: edit.name.value, version: edit.version.value,
              category: edit.category.value, description: edit.description.value,
              isActive: edit.active.checked }) });
          message.className = "form-message success"; message.textContent = t("updateComplete");
          setTimeout(renderRoute, 200);
        } catch (error) { message.className = "form-message error"; message.textContent = error.message; save.disabled = false; }
      });

      const replacement = document.createElement("input");
      replacement.type = "file"; replacement.hidden = true; card.append(replacement);
      replaceButton.addEventListener("click", () => replacement.click());
      replacement.addEventListener("change", async () => {
        if (!replacement.files?.[0]) return;
        replaceButton.disabled = true;
        const body = new FormData(); body.append("file", replacement.files[0]);
        try {
          await api(`/api/admin/downloads/${download.id}/replace`, { method: "POST",
            headers: { "X-CSRF-Token": state.account.csrfToken }, body });
          setTimeout(renderRoute, 150);
        } catch (error) { actionMessage.className = "form-message error admin-action-message"; actionMessage.textContent = error.message; replaceButton.disabled = false; }
      });
      toggleButton.addEventListener("click", async () => {
        toggleButton.disabled = true;
        try {
          await api(`/api/admin/downloads/${download.id}`, { method: "PUT",
            headers: { "Content-Type": "application/json", "X-CSRF-Token": state.account.csrfToken },
            body: JSON.stringify({ isActive: !download.isActive }) });
          renderRoute();
        } catch (error) { actionMessage.className = "form-message error admin-action-message"; actionMessage.textContent = error.message; toggleButton.disabled = false; }
      });
      deleteButton.addEventListener("click", async () => {
        if (!confirm(t("confirmDelete"))) return;
        deleteButton.disabled = true;
        try {
          await api(`/api/admin/downloads/${download.id}`, { method: "DELETE",
            headers: { "X-CSRF-Token": state.account.csrfToken } });
          renderRoute();
        } catch (error) { actionMessage.className = "form-message error admin-action-message"; actionMessage.textContent = error.message; deleteButton.disabled = false; }
      });
      list.append(card);
    });
    page.append(list);
  }

  function stopChatPolling() {
    state.chatPollGeneration += 1;
    if (state.chatPollTimer !== null) clearTimeout(state.chatPollTimer);
    state.chatPollTimer = null;
  }

  function chatMessageRow(message) {
    const row = node("article", "chat-message");
    const profile = node("a", "chat-avatar-link");
    profile.href = `/player/${encodeURIComponent(message.username)}`;
    profile.append(avatar(message));
    const content = node("div", "chat-message-content");
    const header = node("div", "chat-message-header");
    const author = node("a", "chat-author", message.username);
    author.href = profile.href;
    const presence = node("span", `status-dot${message.online ? " online" : ""}`);
    presence.setAttribute("aria-label", message.online ? t("online") : t("offline"));
    const time = node("time", "chat-time", localTime(message.createdAt));
    time.dateTime = message.createdAt;
    header.append(presence, author, time);
    content.append(header, node("p", "chat-text", message.message));
    row.append(profile, content);
    return row;
  }

  async function renderCommunion() {
    if (!state.chatEnabled) throw new Error(t("unavailable"));
    const generation = ++state.chatPollGeneration;
    const page = setPage("page inner-page communion-page");
    titleBlock(page, "communionTitle", "communionLead");
    const [initial, status] = await Promise.all([
      api("/api/chat/messages"), state.websiteStatus ? Promise.resolve(state.websiteStatus) : api("/api/status")
    ]);
    if (generation !== state.chatPollGeneration || location.pathname !== "/communion") return;
    state.websiteStatus = status;

    const panel = node("section", "chat-panel");
    const header = node("div", "chat-panel-header");
    header.append(node("span", "chat-online", t("huntersListening", { n: formatNumber(status.huntersOnline) })),
      node("span", "chat-reset-note", t("chatReset", { n: initial.resetHours })));
    const scroller = node("div", "chat-scroll");
    const list = node("div", "chat-list");
    scroller.append(list);
    const knownIds = new Set();
    let lastId = 0;

    const appendMessages = (messages, forceScroll = false) => {
      const nearBottom = scroller.scrollHeight - scroller.scrollTop - scroller.clientHeight < 96;
      const empty = list.querySelector(".empty-state");
      if (messages.length && empty) empty.remove();
      messages.forEach((message) => {
        const id = Number(message.id);
        if (!Number.isFinite(id) || knownIds.has(id)) return;
        knownIds.add(id);
        lastId = Math.max(lastId, id);
        list.append(chatMessageRow(message));
      });
      if (!list.children.length) list.append(node("div", "empty-state", t("noChatMessages")));
      if (forceScroll || nearBottom) scroller.scrollTop = scroller.scrollHeight;
    };
    appendMessages(initial.messages, true);
    panel.append(header, scroller);

    if (state.account) {
      const form = node("form", "chat-composer");
      const input = document.createElement("textarea");
      input.className = "chat-input";
      input.rows = 2;
      input.maxLength = initial.maxMessageLength;
      input.placeholder = t("chatPlaceholder");
      input.setAttribute("aria-label", t("chatPlaceholder"));
      const send = node("button", "button primary chat-send", t("send"));
      send.type = "submit";
      const feedback = node("p", "form-message chat-feedback");
      form.append(input, send, feedback);
      const submit = async () => {
        if (!input.value.trim() || send.disabled) return;
        send.disabled = true;
        feedback.textContent = "";
        try {
          const message = await api("/api/chat/messages", {
            method: "POST",
            headers: { "Content-Type": "application/json", "X-CSRF-Token": state.account.csrfToken },
            body: JSON.stringify({ message: input.value })
          });
          input.value = "";
          appendMessages([message], true);
        } catch (error) {
          feedback.className = "form-message error chat-feedback";
          feedback.textContent = error.code === "rate_limited" ? t("chatTooFast") : error.message;
        } finally { send.disabled = false; }
      };
      form.addEventListener("submit", (event) => { event.preventDefault(); submit(); });
      input.addEventListener("keydown", (event) => {
        if (event.key === "Enter" && !event.shiftKey) { event.preventDefault(); submit(); }
      });
      panel.append(form);
    } else {
      const prompt = node("div", "chat-login-prompt");
      const link = node("a", "text-link", t("loginToChat"));
      link.href = "/login";
      prompt.append(link);
      panel.append(prompt);
    }
    page.append(panel);

    const poll = async () => {
      if (generation !== state.chatPollGeneration || location.pathname !== "/communion") return;
      try {
        const update = await api(`/api/chat/messages?after=${lastId}`);
        if (generation === state.chatPollGeneration) appendMessages(update.messages);
      } catch (_) {}
      if (generation === state.chatPollGeneration && location.pathname === "/communion") {
        state.chatPollTimer = setTimeout(poll, 2000);
      }
    };
    state.chatPollTimer = setTimeout(poll, 2000);
  }

  function metricList(entries) {
    const list = node("dl", "metric-list");
    entries.forEach(([label, value, pending]) => {
      const item = node("div", "metric"); item.append(node("dt", "", t(label)), node("dd", pending ? "pending" : "", value)); list.append(item);
    });
    return list;
  }

  function renderProfileData(page, player, ownAccount = false) {
    const hero = node("section", "profile-hero"); hero.append(avatar(player, true));
    const identity = node("div"); identity.append(node("p", "eyebrow", ownAccount ? t("accountTitle") : t("profile")), node("h1", "profile-name", player.username), node("div", "profile-meta", statusText(player)));
    hero.append(identity); page.append(hero);
    const details = node("section", "detail-grid");
    const overview = node("article", "detail-section"); overview.append(node("h2", "", t("overview")), metricList([
      ["registered", dateText(player.registeredAt)], ["lastSeen", player.online ? t("online") : relativeTime(player.lastSeen)],
      ["totalSessions", formatNumber(player.totalSessions)], ["totalOnlineTime", formatDuration(player.totalOnlineSeconds)]
    ]));
    const community = node("article", "detail-section"); community.append(node("h2", "", t("community")), metricList([
      ["messagesCreated", formatNumber(player.community.messagesCreated)], ["bloodstainsCreated", formatNumber(player.community.bloodstainsCreated)],
      ["ghostsGenerated", formatNumber(player.community.ghostsGenerated)]
    ]));
    const multiplayer = node("article", "detail-section"); multiplayer.append(node("h2", "", t("multiplayer")), metricList([
      ["activeMatchingRooms", formatNumber(player.multiplayer.activeMatchingRooms)],
      ["summonsAdvertised", formatNumber(player.multiplayer.summonsAdvertised)],
      ["summonClaims", formatNumber(player.multiplayer.summonClaims)],
      ["successfulCoopPending", t("notMeasured"), true]
    ]));
    details.append(overview, community, multiplayer); page.append(details);
  }

  async function renderProfile(username) {
    const player = await api(`/api/players/${encodeURIComponent(username)}`);
    const page = setPage(); renderProfileData(page, player);
  }

  function field(labelKey, name, type, autocomplete, passwordToggle = false) {
    const wrapper = node("div", "field");
    const label = node("label", "", t(labelKey));
    label.htmlFor = `field-${name}`;

    const input = document.createElement("input");
    input.id = `field-${name}`;
    input.name = name;
    input.type = type;
    input.autocomplete = autocomplete;
    input.required = true;

    if (name === "username") {
      input.minLength = 3;
      input.maxLength = 16;
      input.pattern = "[A-Za-z0-9_-]+";
    }

    if (type === "password") {
      input.minLength = 8;
      input.maxLength = 128;
    }

    wrapper.append(label);

    if (type === "password" && passwordToggle) {
      const passwordWrapper = node("div", "password-input-wrapper");

      const toggle = node("button", "password-eye");
      toggle.type = "button";
      toggle.setAttribute("aria-label", t("showPassword"));
      toggle.setAttribute("title", t("showPassword"));
      toggle.setAttribute("aria-pressed", "false");

      toggle.innerHTML = `
        <svg class="eye-icon eye-open" viewBox="0 0 24 24" aria-hidden="true">
          <path d="M2.5 12s3.5-6 9.5-6 9.5 6 9.5 6-3.5 6-9.5 6-9.5-6-9.5-6Z"></path>
          <circle cx="12" cy="12" r="2.8"></circle>
        </svg>

        <svg class="eye-icon eye-closed" viewBox="0 0 24 24" aria-hidden="true">
          <path d="M3 3l18 18"></path>
          <path d="M10.6 6.2A10 10 0 0 1 12 6c6 0 9.5 6 9.5 6s-1.3 2.2-3 3.7"></path>
          <path d="M6.2 6.2C3.8 8 2.5 12 2.5 12s3.5 6 9.5 6a10 10 0 0 0 4.2-.9"></path>
          <path d="M10 10a2.8 2.8 0 0 0 4 4"></path>
        </svg>
      `;

      toggle.addEventListener("click", () => {
        const visible = input.type === "text";

        input.type = visible ? "password" : "text";
        toggle.classList.toggle("showing", !visible);

        const labelText = t(visible ? "showPassword" : "hidePassword");
        toggle.setAttribute("aria-label", labelText);
        toggle.setAttribute("title", labelText);
        toggle.setAttribute("aria-pressed", visible ? "false" : "true");
      });

      passwordWrapper.append(input, toggle);
      wrapper.append(passwordWrapper);
    } else {
      wrapper.append(input);
    }

    return wrapper;
  }

  async function renderRegister() {
    const status = await api("/api/status");
    const page = setPage(); titleBlock(page, "registerTitle", "registerLead");
    if (!status.registrationEnabled) { page.append(node("div", "closed-notice", t("registrationClosed"))); return; }
    const panel = node("section", "form-panel"); const form = node("form", "form-stack");
    form.append(
      field("username", "username", "text", "username"),
      field("password", "password", "password", "new-password", true),
      field("confirmPassword", "confirmPassword", "password", "new-password", true)
    );
    const button = node("button", "button primary", t("create")); button.type = "submit";
    const message = node("p", "form-message");
    const alt = node("p", "form-alternate", t("alreadyAccount") + " "); const link = node("a", "text-link", t("login")); link.href = "/login"; alt.append(link);
    form.append(button, message, alt); panel.append(form); page.append(panel);
    form.addEventListener("submit", async (event) => {
      event.preventDefault(); button.disabled = true; message.className = "form-message"; message.textContent = "";
      const values = Object.fromEntries(new FormData(form));
      try {
        await api("/api/register", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(values) });
        message.classList.add("success"); message.textContent = t("accountCreated"); form.reset();
      } catch (error) { message.classList.add("error"); message.textContent = error.message; }
      finally { button.disabled = false; }
    });
  }

  async function renderLogin() {
    if (state.account) { history.replaceState({}, "", "/account"); return renderAccount(); }
    const page = setPage(); titleBlock(page, "loginTitle", "loginLead");
    const panel = node("section", "form-panel"); const form = node("form", "form-stack");
    form.append(field("username", "username", "text", "username"), field("password", "password", "password", "current-password"));
    const button = node("button", "button primary", t("signIn")); button.type = "submit";
    const message = node("p", "form-message");
    const alt = node("p", "form-alternate", t("newHunter") + " "); const link = node("a", "text-link", t("register")); link.href = "/register"; alt.append(link);
    form.append(button, message, alt); panel.append(form); page.append(panel);
    form.addEventListener("submit", async (event) => {
      event.preventDefault(); button.disabled = true; message.className = "form-message"; message.textContent = "";
      const values = Object.fromEntries(new FormData(form));
      try { await api("/api/login", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(values) }); await refreshAccount(); location.assign("/account"); }
      catch (error) { message.classList.add("error"); message.textContent = error.code === "invalid_credentials" ? t("invalidCredentials") : error.message; button.disabled = false; }
    });
  }

  async function renderAccount() {
    if (!state.account) { history.replaceState({}, "", "/login"); return renderLogin(); }
    const account = await api("/api/account"); state.account = account;
    const page = setPage(); renderProfileData(page, account, true);
    const actions = node("section", "form-panel account-panel"); actions.append(node("h2", "section-title", t("changeAvatar")));
    const form = node("form", "avatar-upload");
    const file = document.createElement("input"); file.type = "file"; file.accept = "image/png,image/jpeg,image/webp"; file.required = true;
    const help = node("p", "form-message", t("avatarHelp")); const message = node("p", "form-message");
    const upload = node("button", "button primary", t("uploadAvatar")); upload.type = "submit";
    form.append(file, help, upload, message); actions.append(form);
    const logout = node("button", "button quiet", t("logout")); logout.type = "button"; actions.append(logout); page.append(actions);
    form.addEventListener("submit", async (event) => {
      event.preventDefault();
      if (!file.files?.[0]) { message.className = "form-message error"; message.textContent = t("chooseImage"); return; }
      upload.disabled = true;
      try {
        await api("/api/account/avatar", { method: "POST", headers: { "Content-Type": file.files[0].type, "X-CSRF-Token": account.csrfToken }, body: file.files[0] });
        message.className = "form-message success"; message.textContent = t("avatarUpdated"); await refreshAccount(); setTimeout(renderRoute, 300);
      } catch (error) { message.className = "form-message error"; message.textContent = error.message; }
      finally { upload.disabled = false; }
    });
    logout.addEventListener("click", async () => {
      try { await api("/api/logout", { method: "POST", headers: { "X-CSRF-Token": account.csrfToken } }); } catch (_) {}
      state.account = null; updateAuthNav(); location.assign("/");
    });
  }

  async function refreshAccount() {
    try { state.account = await api("/api/account"); }
    catch (error) { if (error.status === 401) state.account = null; else throw error; }
    updateAuthNav(); return state.account;
  }

  function updateAuthNav() {
    const link = document.querySelector("[data-auth-link]");
    if (link) {
      link.href = state.account ? "/account" : "/login";
      link.dataset.i18n = state.account ? "account" : "login";
      link.textContent = t(link.dataset.i18n);
    }
    const adminLink = document.querySelector("[data-admin-link]");
    if (adminLink) adminLink.hidden = state.account?.isAdmin !== true;
  }

  function updateChatNav() {
    const link = document.querySelector("[data-chat-link]");
    if (link) link.hidden = !state.chatEnabled;
  }

  async function refreshWebsiteStatus() {
    state.websiteStatus = await api("/api/status");
    state.chatEnabled = state.websiteStatus.chatEnabled === true;
    updateChatNav();
  }

  function activateMotion(root) {
    const items = [...root.querySelectorAll([
      ".hero > *", ".stats-strip", ".panel", ".player-card", ".chalice-filters",
      ".chalice-summary", ".chalice-row:not(.chalice-head)", ".chalice-hero",
      ".detail-section", ".dungeon-map-placeholder", ".download-card",
      ".admin-download-row", ".form-panel", ".chat-panel", ".pagination"
    ].join(","))];
    items.forEach((item, index) => {
      item.classList.add("motion-item");
      item.style.setProperty("--motion-order", String(Math.min(index, 8)));
    });

    const reduced = window.matchMedia?.("(prefers-reduced-motion: reduce)").matches;
    if (reduced || !("IntersectionObserver" in window)) {
      items.forEach((item) => item.classList.add("is-visible"));
      return;
    }

    state.revealObserver = new IntersectionObserver((entries, observer) => {
      entries.forEach((entry) => {
        if (!entry.isIntersecting) return;
        entry.target.classList.add("is-visible");
        observer.unobserve(entry.target);
      });
    }, { threshold: 0.08, rootMargin: "0px 0px -24px" });
    items.forEach((item) => state.revealObserver.observe(item));
  }

  async function renderRoute() {
    stopChatPolling();
    document.querySelector(".site-nav")?.classList.remove("open");
    document.querySelector(".nav-toggle")?.setAttribute("aria-expanded", "false");
    try {
      const path = location.pathname;
      document.querySelectorAll(".site-nav a").forEach((link) => {
        const href = link.getAttribute("href");
        const current = href === path ||
          (href === "/players" && path.startsWith("/player/")) ||
          (href === "/chalice" && path.startsWith("/chalice/")) ||
          (href === "/downloads" && path === "/downloads") ||
          (href === "/admin/downloads" && path === "/admin/downloads");
        if (current) link.setAttribute("aria-current", "page");
        else link.removeAttribute("aria-current");
      });
      if (path === "/") await renderHome();
      else if (path === "/players") await renderPlayers();
      else if (path.startsWith("/player/")) await renderProfile(decodeURIComponent(path.slice(8)));
      else if (path === "/chalice") await renderChalices();
      else if (path.startsWith("/chalice/")) await renderChaliceDetail(decodeURIComponent(path.slice(9)));
      else if (path === "/downloads") await renderDownloads();
      else if (path === "/admin/downloads") await renderAdminDownloads();
      else if (path === "/register") await renderRegister();
      else if (path === "/login") await renderLogin();
      else if (path === "/account") await renderAccount();
      else if (path === "/communion") await renderCommunion();
      else showError(new Error("Not found"));
      if (location.hash) {
        const reduced = window.matchMedia?.("(prefers-reduced-motion: reduce)").matches;
        document.querySelector(location.hash)?.scrollIntoView({ behavior: reduced ? "auto" : "smooth" });
      }
    } catch (error) { showError(error); }
    applyLanguage();
    activateMotion(app);
  }

  document.querySelectorAll("[data-language]").forEach((button) => button.addEventListener("click", () => {
    state.language = button.dataset.language; localStorage.setItem("thr-language", state.language); applyLanguage(); renderRoute();
  }));
  const toggle = document.querySelector(".nav-toggle");
  toggle?.addEventListener("click", () => {
    const nav = document.querySelector(".site-nav"); const open = nav.classList.toggle("open"); toggle.setAttribute("aria-expanded", open ? "true" : "false");
  });
  document.addEventListener("keydown", (event) => {
    if (event.key !== "Escape") return;
    const nav = document.querySelector(".site-nav");
    if (!nav?.classList.contains("open")) return;
    nav.classList.remove("open");
    toggle?.setAttribute("aria-expanded", "false");
    toggle?.focus();
  });
  document.addEventListener("click", (event) => {
    const anchor = event.target.closest("a[href]");
    if (!anchor || anchor.origin !== location.origin || anchor.target || event.ctrlKey || event.metaKey || event.shiftKey) return;
    const url = new URL(anchor.href); if (url.pathname.startsWith("/api/") || url.pathname.startsWith("/assets/") || url.pathname.startsWith("/avatars/") || url.pathname.startsWith("/downloads/file/")) return;
    event.preventDefault(); history.pushState({}, "", url.pathname + url.search + url.hash); renderRoute();
  });
  addEventListener("popstate", renderRoute);

  applyLanguage();
  Promise.allSettled([refreshAccount(), refreshWebsiteStatus()]).finally(renderRoute);
})();
