// SPDX-FileCopyrightText: Copyright 2026 shadNet Project
// SPDX-License-Identifier: GPL-2.0-or-later
"use strict";

(() => {
  const translations = {
    en: {
      menu: "Menu", home: "Home", hunters: "Hunters", stats: "Stats", register: "Register",
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
      exactMetrics: "Live values derived directly from authenticated clients, accounts, Matching2 rooms and stored messages."
    },
    es: {
      menu: "Menú", home: "Inicio", hunters: "Cazadores", stats: "Estadísticas", register: "Registro",
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
      exactMetrics: "Valores en vivo obtenidos directamente de clientes autenticados, cuentas, salas Matching2 y mensajes almacenados."
    }
  };

  const state = {
    language: localStorage.getItem("thr-language") === "en" ? "en" : "es",
    account: null
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

  function setPage(markupClass = "page inner-page") {
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

    const [status, playersData, activityData] = await Promise.all([
      api("/api/status"), api("/api/players?online=true&limit=6"), api("/api/activity?limit=6")
    ]);
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
    onlinePanel.append(onlineList); grid.append(activityPanel, onlinePanel); page.append(grid);
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

  function field(labelKey, name, type, autocomplete) {
    const wrapper = node("div", "field");
    const label = node("label", "", t(labelKey)); label.htmlFor = `field-${name}`;
    const input = document.createElement("input"); input.id = `field-${name}`; input.name = name; input.type = type; input.autocomplete = autocomplete; input.required = true;
    if (name === "username") { input.minLength = 3; input.maxLength = 16; input.pattern = "[A-Za-z0-9_-]+"; }
    if (type === "password") { input.minLength = 8; input.maxLength = 128; }
    wrapper.append(label, input); return wrapper;
  }

  async function renderRegister() {
    const status = await api("/api/status");
    const page = setPage(); titleBlock(page, "registerTitle", "registerLead");
    if (!status.registrationEnabled) { page.append(node("div", "closed-notice", t("registrationClosed"))); return; }
    const panel = node("section", "form-panel"); const form = node("form", "form-stack");
    form.append(field("username", "username", "text", "username"), field("password", "password", "password", "new-password"), field("confirmPassword", "confirmPassword", "password", "new-password"));
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
    if (!link) return;
    link.href = state.account ? "/account" : "/login";
    link.dataset.i18n = state.account ? "account" : "login";
    link.textContent = t(link.dataset.i18n);
  }

  async function renderRoute() {
    document.querySelector(".site-nav")?.classList.remove("open");
    try {
      const path = location.pathname;
      document.querySelectorAll(".site-nav a").forEach((link) => {
        const href = link.getAttribute("href");
        link.toggleAttribute("aria-current", href === path || (href === "/players" && path.startsWith("/player/")));
      });
      if (path === "/") await renderHome();
      else if (path === "/players") await renderPlayers();
      else if (path.startsWith("/player/")) await renderProfile(decodeURIComponent(path.slice(8)));
      else if (path === "/register") await renderRegister();
      else if (path === "/login") await renderLogin();
      else if (path === "/account") await renderAccount();
      else showError(new Error("Not found"));
      if (location.hash) document.querySelector(location.hash)?.scrollIntoView({ behavior: "smooth" });
    } catch (error) { showError(error); }
    applyLanguage();
  }

  document.querySelectorAll("[data-language]").forEach((button) => button.addEventListener("click", () => {
    state.language = button.dataset.language; localStorage.setItem("thr-language", state.language); applyLanguage(); renderRoute();
  }));
  const toggle = document.querySelector(".nav-toggle");
  toggle?.addEventListener("click", () => {
    const nav = document.querySelector(".site-nav"); const open = nav.classList.toggle("open"); toggle.setAttribute("aria-expanded", open ? "true" : "false");
  });
  document.addEventListener("click", (event) => {
    const anchor = event.target.closest("a[href]");
    if (!anchor || anchor.origin !== location.origin || anchor.target || event.ctrlKey || event.metaKey || event.shiftKey) return;
    const url = new URL(anchor.href); if (url.pathname.startsWith("/api/") || url.pathname.startsWith("/assets/") || url.pathname.startsWith("/avatars/")) return;
    event.preventDefault(); history.pushState({}, "", url.pathname + url.search + url.hash); renderRoute();
  });
  addEventListener("popstate", renderRoute);

  applyLanguage();
  refreshAccount().catch(() => { state.account = null; updateAuthNav(); }).finally(renderRoute);
})();
