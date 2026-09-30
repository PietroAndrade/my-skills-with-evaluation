# Worked examples: user story to feature file

Two real cases, each with what an unaided model wrote and what the behaviour
actually was. Both come from a measured run
(`docs/bdd-gherkin-style-story-benchmark.md`); the "Instead of" columns are not
invented for illustration, they are what was produced.

A user story is a harder input than a leaked feature file. With a leaked file the
mechanism is visible and you delete it. With a story you must *invent* the
detail, and the nearest vocabulary is usually the implementation's.

---

## Example 1 — a rule that enables access

> As a network admin, I want to connect in my appliance by rdp,
> For it I need a rule enabling this external access

### Instead of

```gherkin
  Scenario: Create a firewall rule enabling external RDP access
    Given no firewall rule allows external RDP traffic
    When I create a firewall rule with the following attributes:
      | attribute   | value         |
      | name        | Allow-RDP-WAN |
      | direction   | inbound       |
      | source      | any           |
      | protocol    | TCP           |
      | port        | 3389          |
      | action      | allow         |
    And I apply the firewall configuration
    Then the rule "Allow-RDP-WAN" appears in the firewall rule list as enabled
    And the firewall log records the allowed connection for rule "Allow-RDP-WAN"
```

Three separate failures, and the first is the one that matters:

1. **The asserted outcome is that a form was filled in.** This scenario passes
   while nobody can connect. The story asked for access; the file promises a row
   in a list.
2. The data table is the rule-creation screen transcribed into the spec. It
   breaks when a field is renamed, and it tells a reviewer nothing about
   behaviour.
3. The log assertion couples the spec to a logging decision the team can change
   on a Tuesday.

### Write

```gherkin
  Scenario Outline: The source address allowed by the rule decides who may connect
    Given a rule that allows remote desktop access to the appliance from <allowed_source>
    When a network admin opens a remote desktop session to the appliance from <admin_source>
    Then access to the appliance is <result>

    Examples:
      | allowed_source  | admin_source  | result  |
      | 198.51.100.7    | 198.51.100.7  | granted |
      | 198.51.100.0/24 | 198.51.100.7  | granted |
      | 198.51.100.0/24 | 192.0.2.55    | denied  |
```

The rule is still there — it is the `Given`. What changed is that the *outcome*
is the access, which is what the admin wanted and what a reviewer can judge.

**A note on the port.** The version above says "remote desktop" and drops 3389.
That is a defensible reading and it was also a loss: 3389 is a public fact of the
protocol, the same category as an HTTP status in a published contract, and for a
spec a network admin reviews it may be the most reviewable detail of all. Naming
it is not leakage. Deciding to hide it should be a decision, not a reflex.

---

## Example 2 — knowing whether a device is configured correctly

> Context: configuration of OS properties via REST API and websocket.
>
> As a user, I want to know if my device is correctly configurated,
> so that I can ensure only one audio source was seted

### Instead of

```gherkin
  Scenario: Reading the configuration of a correctly configured device
    Given the device has the audio source "hdmi" active
    When I send a GET request to "/api/v1/os/properties/audio"
    Then the response status code is 200
    And the response field "activeSource" is "hdmi"
    And the response list "activeSources" has 1 item

  Scenario: Receiving the configuration state after subscribing
    When I open a websocket connection to "/ws/v1/os/properties"
    And I subscribe to the topic "audio"
    Then I receive a message of type "audio.state" within 5 seconds
```

**A context line naming a transport is describing the *how*.** "via REST API and
websocket" tells you how the configuration is carried, not what the user wants.
The user wants *to know* whether the device is configured correctly. Here the
transport became the subject, and the file turned into an API test suite: routes,
verbs, status codes, payload field names, topic names, frame types, timeouts.

Ask the ownership question from the main skill: is this something the consumer
depends on, or something the team chose? If this user is an integrator building
against a published API, the routes and status codes *are* the contract and
belong. If this user is an operator who wants a correct device, they are
plumbing. The story does not say — so say which reading you took, rather than
defaulting to the transport because it was mentioned.

### Write

```gherkin
  Scenario: A freshly configured device reports exactly one active audio source
    Given the built-in microphone is the active audio source
    When a user asks the device for its audio configuration
    Then the device reports the built-in microphone as the active audio source
    And the device reports no other active audio source

  Scenario: A user watching the device sees the audio source change as it happens
    Given the built-in microphone is the active audio source
    And a user is watching the device configuration for changes
    When another user selects the headset microphone as the audio source
    Then the watching user is told the headset microphone is the active audio source
    And the device reports no other active audio source
```

`And the device reports no other active audio source` is reused character for
character everywhere the invariant is asserted — one step definition instead of
five, and the "only one" from the story is now checked rather than implied.

Note the second scenario: the websocket did not disappear, it became "a user is
watching … for changes". That keeps it true if the push mechanism changes.

**Two things this version got wrong, worth copying as warnings rather than as
style.** It dropped the zero-active-sources case, which the transport-heavy
version covered — a device with no audio source is as misconfigured as one with
two, and "only one" has two failure directions. And if the websocket exists
*because* polling was not good enough, then push-versus-poll is itself behaviour
and abstracting it away loses a real requirement.

---

## The move both examples share

In both, the unaided version asserted **that the system had been configured**.
The behaviour is **what the configuration now lets someone do**.

| The story asks for | The trap is to assert | Assert instead |
|---|---|---|
| a rule enabling access | the rule exists and is enabled | the access succeeds, and is denied without it |
| knowing the device is correct | the response field says so | the device reports what is active, and nothing else is |

If a scenario would still pass after the feature stopped working for its user,
it is asserting the mechanism.

---

## Spotting a story that will go technical

Both examples above came out technical unaided for the same reason, and the
reason is predictable from the story before anything is written.

**Every domain has a substrate** — the vocabulary for how the system stores,
transports and manipulates the thing, as opposed to what happens to someone. A
firewall rule's substrate is the rule list, the apply action and the log line. A
refund's is the payments table and the ledger entry. What varies is how close it
sits to the words the story used.

Ask: **can this story be satisfied by changing a configuration and showing the
configuration changed?**

- *"a rule enabling this access"* — a rule is a row in a list. Yes. Expect the pull.
- *"only one audio source was set"* — a setting is a field. Yes. Expect the pull.
- *"compare the current run against the baseline"* — a comparison is a judgement,
  stored nowhere. No. Little to fall into.

When the answer is yes, the concrete layer of the problem *is* the
implementation, and writing the domain layer takes deliberate effort. The rule
belongs in the `Given`; the outcome is what the configuration now lets someone
do.

**A transport named anywhere in the input is the strongest warning sign.**
Example 2's "via REST API and websocket" was written as background and pulled the
entire file into it — routes, status codes, payload fields, topic names. If the
brief names a transport or a store, decide explicitly whether the reader is an
integrator building against that contract or an operator who wants a working
device, and say which reading you took.

*(Observed across five measured stories;
`docs/bdd-gherkin-style-story-benchmark.md` has the runs. The three-level
severity ordering there is a post-hoc reading, not a scored result.)*
