// The tab strip and toolbar of `Starfish browser`.
//
// The shell owns the tab state; this page draws the copy it is sent and
// turns clicks into commands. The messages in both directions are listed
// in src/shell/browser/BrowserUIChannel.h and must match it:
//
//   to the shell   shell.newTab(), shell.closeTab(id), shell.activateTab(id),
//                  shell.navigate(text), shell.goBack(), shell.goForward(),
//                  shell.reload()
//   from the shell browserUI.tabsChanged(tabs, activeId),
//                  browserUI.tabUpdated(tab), browserUI.focusAddress()
//
// A tab is { id, url, title, isLoading, canGoBack, canGoForward }.
var browserUI = (function() {
    var tabs = [];
    var activeId = 0;

    var tabsElement = document.getElementById('tabs');
    var address = document.getElementById('address');
    var back = document.getElementById('back');
    var forward = document.getElementById('forward');

    // A shell function takes exactly one string.
    function send(command, argument) {
        shell[command](argument === undefined ? '' : String(argument));
    }

    function activeTab() {
        for (var i = 0; i < tabs.length; i++) {
            if (tabs[i].id === activeId) {
                return tabs[i];
            }
        }
        return null;
    }

    function tabLabel(tab) {
        return tab.title || tab.url || 'New tab';
    }

    function createTabElement(tab) {
        var element = document.createElement('div');
        element.className = 'tab' + (tab.id === activeId ? ' active' : '') +
            (tab.isLoading ? ' loading' : '');
        element.setAttribute('data-tab', tab.id);

        var title = document.createElement('span');
        title.className = 'title';
        title.textContent = tabLabel(tab);
        element.appendChild(title);

        var close = document.createElement('button');
        close.className = 'close';
        close.textContent = 'x';
        close.title = 'Close tab (Ctrl+W)';
        element.appendChild(close);

        element.addEventListener('mousedown', function(event) {
            if (event.target !== close) {
                send('activateTab', tab.id);
            }
        });
        close.addEventListener('click', function() {
            send('closeTab', tab.id);
        });
        return element;
    }

    function renderTabs() {
        while (tabsElement.firstChild) {
            tabsElement.removeChild(tabsElement.firstChild);
        }
        for (var i = 0; i < tabs.length; i++) {
            tabsElement.appendChild(createTabElement(tabs[i]));
        }
    }

    // The toolbar describes the active tab. Text being typed is kept.
    function renderToolbar(keepAddress) {
        var tab = activeTab();
        back.disabled = !tab || !tab.canGoBack;
        forward.disabled = !tab || !tab.canGoForward;
        if (!keepAddress) {
            var url = tab ? tab.url : '';
            address.value = url === 'about:blank' ? '' : url;
        }
    }

    function isEditingAddress() {
        return document.activeElement === address;
    }

    document.getElementById('new-tab').addEventListener('click', function() {
        send('newTab');
    });
    back.addEventListener('click', function() {
        send('goBack');
    });
    forward.addEventListener('click', function() {
        send('goForward');
    });
    document.getElementById('reload').addEventListener('click', function() {
        send('reload');
    });
    address.addEventListener('keydown', function(event) {
        if (event.key === 'Enter' && address.value) {
            address.blur();
            send('navigate', address.value);
        } else if (event.key === 'Escape') {
            address.blur();
            renderToolbar(false);
        }
    });

    return {
        tabsChanged: function(newTabs, newActiveId) {
            var isSameTab = newActiveId === activeId;
            tabs = newTabs;
            activeId = newActiveId;
            renderTabs();
            renderToolbar(isSameTab && isEditingAddress());
        },

        tabUpdated: function(tab) {
            for (var i = 0; i < tabs.length; i++) {
                if (tabs[i].id === tab.id) {
                    tabs[i] = tab;
                }
            }
            renderTabs();
            if (tab.id === activeId) {
                renderToolbar(isEditingAddress());
            }
        },

        focusAddress: function() {
            address.focus();
            address.select();
        }
    };
})();
