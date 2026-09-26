# Web Presentation Patterns

Patterns for organizing how web requests are received, processed, and responded to.

Source: https://martinfowler.com/eaaCatalog/

---

## Model View Controller (MVC)

**Problem**: UI code, business logic, and data access are intertwined — every change to the UI requires touching business logic and vice versa.

**When to Use**:
- Almost any interactive application with a UI layer
- When the same data needs to be displayed in multiple ways
- When you want to test logic independently of rendering

**Structure**:
- **Model**: holds data and business logic; knows nothing about views
- **View**: renders the model for display; reads the model but does not mutate it
- **Controller**: receives user input, updates the model, selects the next view

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>

// ── Model ────────────────────────────────────────────────────────────────────
class ProductModel {
    struct Product { int id; std::string name; double price; };
    std::vector<Product> products = {
        { 1, "Widget",  9.99  },
        { 2, "Gadget",  24.99 },
        { 3, "Doohickey", 4.99 },
    };

public:
    const std::vector<Product>& getAll() const { return products; }

    std::vector<Product> search(const std::string& term) const {
        std::vector<Product> result;
        for (const auto& p : products)
            if (p.name.find(term) != std::string::npos) result.push_back(p);
        return result;
    }
};

// ── View ─────────────────────────────────────────────────────────────────────
class ProductListView {
public:
    void render(const std::vector<ProductModel::Product>& items) const {
        std::cout << "=== Product List ===\n";
        if (items.empty()) {
            std::cout << "  (no products found)\n";
            return;
        }
        for (const auto& p : items)
            std::cout << "  [" << p.id << "] " << p.name
                      << " — $" << p.price << "\n";
    }
};

// ── Controller ───────────────────────────────────────────────────────────────
class ProductController {
    ProductModel&    model;
    ProductListView& view;
public:
    ProductController(ProductModel& m, ProductListView& v)
        : model(m), view(v) {}

    // Handle request: list all products
    void listAll() {
        view.render(model.getAll());
    }

    // Handle request: search by name
    void search(const std::string& term) {
        auto results = model.search(term);
        view.render(results);
    }
};

int main() {
    ProductModel    model;
    ProductListView view;
    ProductController controller(model, view);

    controller.listAll();
    std::cout << "\nSearch results for 'get':\n";
    controller.search("get");
}
```

**Pros**:
- Clear separation of concerns — UI changes don't affect model
- Multiple views can share the same model
- Model is easily unit-testable

**Cons**:
- Can over-engineer for simple UIs
- Controller can become fat if it takes on too much
- Observers/callbacks needed to keep views in sync

**Related**: Front Controller (centralizes all request routing), Page Controller (one controller per page)

---

## Front Controller

**Problem**: Each page/request handler duplicates cross-cutting concerns (authentication, logging, routing), leading to scattered, inconsistent behavior.

**When to Use**:
- Web applications with many request types
- When security, logging, or routing should be applied uniformly
- To centralize the input funnel and dispatch to handlers

**Structure**:
- A single controller (or a dispatcher + command chain) handles ALL incoming requests
- It parses the request, performs cross-cutting work, then dispatches to specific action handlers
- Similar to the Chain of Responsibility combined with Command pattern

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

// HTTP-like request/response stubs
struct HttpRequest  { std::string path; std::string user; };
struct HttpResponse { std::string body; int status = 200; };

// Command interface — each action handler
class Command {
public:
    virtual ~Command() = default;
    virtual HttpResponse execute(const HttpRequest& req) = 0;
};

// Concrete commands (page handlers)
class HomeCommand : public Command {
public:
    HttpResponse execute(const HttpRequest& req) override {
        return { "Welcome, " + req.user + "! This is home.", 200 };
    }
};

class ProductListCommand : public Command {
public:
    HttpResponse execute(const HttpRequest& req) override {
        return { "Product list for " + req.user, 200 };
    }
};

class NotFoundCommand : public Command {
public:
    HttpResponse execute(const HttpRequest& req) override {
        return { "404 Not Found: " + req.path, 404 };
    }
};

// Front Controller — single entry point
class FrontController {
    std::unordered_map<std::string, std::shared_ptr<Command>> routes;
    std::shared_ptr<Command> notFound;

    // Cross-cutting: authentication check
    bool authenticate(const HttpRequest& req) {
        if (req.user.empty()) {
            std::cout << "[Auth] Rejected anonymous request\n";
            return false;
        }
        std::cout << "[Auth] Authenticated: " << req.user << "\n";
        return true;
    }

    // Cross-cutting: logging
    void log(const HttpRequest& req, const HttpResponse& resp) {
        std::cout << "[Log] " << req.path << " -> " << resp.status << "\n";
    }

public:
    FrontController() : notFound(std::make_shared<NotFoundCommand>()) {
        routes["/"]         = std::make_shared<HomeCommand>();
        routes["/products"] = std::make_shared<ProductListCommand>();
    }

    HttpResponse handle(const HttpRequest& req) {
        if (!authenticate(req))
            return { "Unauthorized", 401 };

        auto it = routes.find(req.path);
        Command* cmd = (it != routes.end()) ? it->second.get() : notFound.get();

        HttpResponse resp = cmd->execute(req);
        log(req, resp);
        return resp;
    }
};

int main() {
    FrontController fc;

    auto resp1 = fc.handle({ "/", "alice" });
    std::cout << "Response: " << resp1.body << "\n\n";

    auto resp2 = fc.handle({ "/products", "bob" });
    std::cout << "Response: " << resp2.body << "\n\n";

    auto resp3 = fc.handle({ "/unknown", "carol" });
    std::cout << "Response: " << resp3.body << " (" << resp3.status << ")\n\n";

    auto resp4 = fc.handle({ "/", "" });  // anonymous
    std::cout << "Response: " << resp4.body << " (" << resp4.status << ")\n";
}
```

**Pros**:
- Single place to add cross-cutting concerns (auth, logging, session)
- Easy to add new routes without touching existing code
- Consistent request handling

**Cons**:
- Can become a bottleneck or monolith
- All traffic flows through one point — must be fast and correct

**Related**: MVC (MVC Controller is usually behind a Front Controller), Page Controller (alternative: one class per URL), Command (dispatched by Front Controller)

---

## Page Controller

**Problem**: Front Controller can be overkill for simple apps. You want one class per URL or action rather than a central dispatcher.

**When to Use**:
- Small or medium web applications with a moderate number of pages
- When each page/action has distinctly different logic
- When simplicity matters more than uniformity

**Structure**:
- One controller class per page or URL pattern
- Each class handles its own request parsing, model interaction, and view selection
- May share a base class for common infrastructure (auth, error handling)

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <memory>

struct HttpRequest  { std::string path; std::string queryParam; std::string user; };
struct HttpResponse { std::string body; int status = 200; };

// Base page controller — shared infrastructure
class PageController {
protected:
    std::string currentUser;

    virtual HttpResponse handleRequest(const HttpRequest& req) = 0;

    bool isAuthenticated(const HttpRequest& req) {
        return !req.user.empty();
    }

public:
    virtual ~PageController() = default;

    HttpResponse process(const HttpRequest& req) {
        currentUser = req.user;
        if (!isAuthenticated(req))
            return { "Unauthorized", 401 };
        return handleRequest(req);
    }
};

// One controller per page
class ArticleController : public PageController {
    HttpResponse handleRequest(const HttpRequest& req) override {
        std::string articleId = req.queryParam;
        if (articleId.empty())
            return { "400 Bad Request: missing article id", 400 };
        return { "Article content for id=" + articleId + " (viewed by " + currentUser + ")", 200 };
    }
};

class UserProfileController : public PageController {
    HttpResponse handleRequest(const HttpRequest& req) override {
        return { "Profile page for " + currentUser, 200 };
    }
};

class CheckoutController : public PageController {
    HttpResponse handleRequest(const HttpRequest& req) override {
        // Complex checkout logic here
        std::cout << "[Checkout] Processing order for " << currentUser << "\n";
        return { "Order confirmed for " + currentUser, 200 };
    }
};

// Simple URL-based dispatcher (could be a Front Controller or a framework)
std::unique_ptr<PageController> resolveController(const std::string& path) {
    if (path == "/article")  return std::make_unique<ArticleController>();
    if (path == "/profile")  return std::make_unique<UserProfileController>();
    if (path == "/checkout") return std::make_unique<CheckoutController>();
    return nullptr;
}

int main() {
    std::vector<HttpRequest> requests = {
        { "/article",  "42",  "alice" },
        { "/profile",  "",    "bob"   },
        { "/checkout", "",    "carol" },
        { "/article",  "",    ""      }, // unauthorized
    };

    for (const auto& req : requests) {
        auto controller = resolveController(req.path);
        if (!controller) {
            std::cout << req.path << " -> 404 Not Found\n";
            continue;
        }
        auto resp = controller->process(req);
        std::cout << req.path << " -> [" << resp.status << "] " << resp.body << "\n";
    }
}
```

**Pros**:
- Simple — each page lives in its own class
- Easy to find logic for a specific URL
- Low coupling between pages

**Cons**:
- Cross-cutting concerns require base class or duplication
- No single place for routing or request pre-processing
- Scales awkwardly as pages multiply

**Related**: Front Controller (centralized alternative), MVC (can wrap each page controller), Template View (view to pair with each controller)

---

## Template View

**Problem**: Embedding rendering logic (HTML generation) directly in controller code creates messy, untestable string concatenation.

**When to Use**:
- When the view layer is HTML or any text-based format
- When designers and developers need to work on views independently
- To separate presentation logic from business logic

**Structure**:
- HTML template with placeholders (markers/slots) for dynamic data
- A rendering engine substitutes data into placeholders at request time
- No business logic in the template — only presentation conditionals and loops

**C++ Example**:
```cpp
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <regex>

// Simple template engine that replaces {{key}} with values
class TemplateView {
    std::string templateText;

    // Replace all {{key}} occurrences with values
    static std::string render(std::string tmpl,
                              const std::unordered_map<std::string, std::string>& vars)
    {
        for (const auto& [key, value] : vars) {
            std::string placeholder = "{{" + key + "}}";
            size_t pos = 0;
            while ((pos = tmpl.find(placeholder, pos)) != std::string::npos) {
                tmpl.replace(pos, placeholder.size(), value);
                pos += value.size();
            }
        }
        return tmpl;
    }

public:
    explicit TemplateView(std::string tmpl) : templateText(std::move(tmpl)) {}

    std::string renderWith(const std::unordered_map<std::string, std::string>& vars) const {
        return render(templateText, vars);
    }
};

// Model
struct UserProfile { std::string name; std::string email; int orderCount; };

// Controller that uses a Template View
class ProfilePageController {
    TemplateView view{
        "<html>\n"
        "  <h1>Welcome, {{name}}!</h1>\n"
        "  <p>Email: {{email}}</p>\n"
        "  <p>You have {{orderCount}} order(s).</p>\n"
        "</html>"
    };

public:
    std::string handle(const std::string& userId) {
        // Fetch model (simulated)
        UserProfile profile{ "Alice", "alice@example.com", 5 };

        // Bind model to template
        return view.renderWith({
            { "name",       profile.name                        },
            { "email",      profile.email                       },
            { "orderCount", std::to_string(profile.orderCount)  },
        });
    }
};

int main() {
    ProfilePageController controller;
    std::string html = controller.handle("user-1");
    std::cout << html << "\n";
}
```

**Pros**:
- Clear separation of HTML from logic
- Designers can edit templates independently
- Templates are readable and maintainable

**Cons**:
- Complex loops or conditionals in templates can grow messy
- No static type checking for template variables
- Logic often bleeds into templates as apps grow

**Related**: Page Controller / Front Controller (provide the controller that feeds the template), MVC (Template View is typically the V)
