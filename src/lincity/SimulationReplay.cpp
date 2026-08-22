#include "SimulationReplay.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <sstream>

#include "groups.hpp"
#include "lintypes.hpp"
#include "messages.hpp"
#include "modules/rocket_pad.hpp"

namespace {

template<typename T>
T parseInteger(std::string_view value, const char* field) {
  T result{};
  const char* begin = value.data();
  const char* end = begin + value.size();
  const auto parsed = std::from_chars(begin, end, result);
  if(parsed.ec != std::errc() || parsed.ptr != end)
    throw SimulationReplayError(std::string("invalid ") + field);
  return result;
}

std::vector<std::string_view> split(std::string_view line) {
  std::vector<std::string_view> fields;
  while(true) {
    const std::size_t tab = line.find('\t');
    fields.push_back(line.substr(0, tab));
    if(tab == std::string_view::npos)
      return fields;
    line.remove_prefix(tab + 1);
  }
}

bool validToken(std::string_view token) {
  return !token.empty() && std::all_of(token.begin(), token.end(), [](char c) {
    const unsigned char byte = static_cast<unsigned char>(c);
    return std::isalnum(byte) || c == '-' || c == '_' || c == '.'
      || c == '/' || c == ':';
  });
}

const char* typeName(SimulationCommandType type) {
  switch(type) {
    case SimulationCommandType::BUILD: return "build";
    case SimulationCommandType::BULLDOZE: return "bulldoze";
    case SimulationCommandType::EVACUATE: return "evacuate";
    case SimulationCommandType::FLOOD: return "flood";
    case SimulationCommandType::SET_MONEY_RATE: return "set-rate";
    case SimulationCommandType::COAL_SURVEY: return "coal-survey";
    case SimulationCommandType::LAUNCH_ROCKET: return "launch-rocket";
  }
  throw SimulationReplayError("unknown command type");
}

SimulationCommandType parseType(std::string_view value) {
  if(value == "build") return SimulationCommandType::BUILD;
  if(value == "bulldoze") return SimulationCommandType::BULLDOZE;
  if(value == "evacuate") return SimulationCommandType::EVACUATE;
  if(value == "flood") return SimulationCommandType::FLOOD;
  if(value == "set-rate") return SimulationCommandType::SET_MONEY_RATE;
  if(value == "coal-survey") return SimulationCommandType::COAL_SURVEY;
  if(value == "launch-rocket") return SimulationCommandType::LAUNCH_ROCKET;
  throw SimulationReplayError("unknown command type");
}

const char* outcomeName(SimulationCommandOutcome outcome) {
  return outcome == SimulationCommandOutcome::APPLIED ? "applied" : "no-change";
}

SimulationCommandOutcome parseOutcome(std::string_view value) {
  if(value == "applied") return SimulationCommandOutcome::APPLIED;
  if(value == "no-change") return SimulationCommandOutcome::NO_CHANGE;
  throw SimulationReplayError("unknown command outcome");
}

StateFingerprint128 parseFingerprint(std::string_view value) {
  if(value.size() != 32)
    throw SimulationReplayError("fingerprint must contain 32 hex digits");
  auto half = [](std::string_view digits) {
    std::uint64_t result = 0;
    for(char c : digits) {
      result <<= 4U;
      if(c >= '0' && c <= '9') result |= static_cast<unsigned>(c - '0');
      else if(c >= 'a' && c <= 'f')
        result |= static_cast<unsigned>(c - 'a' + 10);
      else throw SimulationReplayError("invalid fingerprint");
    }
    return result;
  };
  return {half(value.substr(0, 16)), half(value.substr(16, 16))};
}

World::MoneyRateAccount moneyRateAccount(int value) {
  if(value < static_cast<int>(World::MoneyRateAccount::INCOME_TAX)
    || value > static_cast<int>(World::MoneyRateAccount::GOODS_TAX))
    throw SimulationReplayError("invalid money-rate account");
  return static_cast<World::MoneyRateAccount>(value);
}

void requireVisible(const World& world, MapPoint point) {
  if(!world.map.is_visible(point))
    throw SimulationReplayError("command point is outside the visible map");
}

void validateReplay(const SimulationReplay& replay) {
  if(replay.version != SimulationReplay::FORMAT_VERSION)
    throw SimulationReplayError("unsupported replay version");
  if(replay.protocol != "legacy")
    throw SimulationReplayError("unsupported replay protocol");
  if(!validToken(replay.origin))
    throw SimulationReplayError("invalid replay origin");

  std::uint32_t previousDay = 0;
  std::uint32_t previousSequence = 0;
  bool first = true;
  for(const SimulationCommand& command : replay.commands) {
    if(command.day == 0)
      throw SimulationReplayError("commands must target day 1 or later");
    if(!first && (command.day < previousDay
      || (command.day == previousDay && command.sequence <= previousSequence)))
      throw SimulationReplayError("commands are not strictly ordered");
    previousDay = command.day;
    previousSequence = command.sequence;
    first = false;
  }
  previousDay = 0;
  for(const SimulationCheckpoint& checkpoint : replay.checkpoints) {
    if(checkpoint.day == 0 || checkpoint.day <= previousDay)
      throw SimulationReplayError("checkpoints are not strictly ordered");
    previousDay = checkpoint.day;
  }
}

} // namespace

SimulationCommandOutcome executeSimulationCommand(
  World& world, const SimulationCommand& command
) {
  if(command.phase != SimulationCommandPhase::BEFORE_STEP)
    throw SimulationReplayError("unsupported command phase");

  try {
  switch(command.type) {
    case SimulationCommandType::BUILD: {
      if(command.argument0 < 0
        || command.argument0 > std::numeric_limits<unsigned short>::max())
        throw SimulationReplayError("invalid construction group id");
      ConstructionGroup* group = ConstructionGroup::getConstructionGroup(
        static_cast<unsigned short>(command.argument0));
      if(!group)
        throw SimulationReplayError("unknown construction group id");
      Message::ptr message;
      if(!group->can_build(world, message)
        || !group->can_build_here(world, command.point, message))
        throw SimulationReplayError("construction command is not allowed");
      world.buildConstruction(*group, command.point);
      return SimulationCommandOutcome::APPLIED;
    }
    case SimulationCommandType::BULLDOZE: {
      requireVisible(world, command.point);
      if(Construction* construction =
          world.map(command.point)->reportingConstruction) {
        Message::ptr message;
        if(!construction->can_bulldoze(message))
          throw SimulationReplayError("construction cannot be bulldozed");
      }
      const unsigned short before = world.map(command.point)->getGroup();
      world.bulldozeArea(command.point);
      return before == GROUP_DESERT
        ? SimulationCommandOutcome::NO_CHANGE
        : SimulationCommandOutcome::APPLIED;
    }
    case SimulationCommandType::EVACUATE: {
      requireVisible(world, command.point);
      Construction* construction = world.map(command.point)->reportingConstruction;
      if(!construction || (construction->flags & FLAG_NEVER_EVACUATE))
        throw SimulationReplayError("construction cannot be evacuated");
      world.evacuateArea(command.point);
      return SimulationCommandOutcome::APPLIED;
    }
    case SimulationCommandType::FLOOD:
      requireVisible(world, command.point);
      if(!world.map(command.point)->is_bare())
        throw SimulationReplayError("flood target is occupied");
      world.floodArea(command.point);
      return SimulationCommandOutcome::APPLIED;
    case SimulationCommandType::SET_MONEY_RATE: {
      const World::MoneyRateAccount account = moneyRateAccount(command.argument0);
      if(command.argument1 < 0
        || command.argument1 > 2 * world.defaultMoneyRate(account))
        throw SimulationReplayError("money rate is outside its allowed range");
      if(world.moneyRate(account) == command.argument1)
        return SimulationCommandOutcome::NO_CHANGE;
      if(!world.setMoneyRate(account, command.argument1))
        throw SimulationReplayError("money rate was rejected");
      return SimulationCommandOutcome::APPLIED;
    }
    case SimulationCommandType::COAL_SURVEY:
      if(world.coal_survey_done)
        return SimulationCommandOutcome::NO_CHANGE;
      world.do_coal_survey();
      return SimulationCommandOutcome::APPLIED;
    case SimulationCommandType::LAUNCH_ROCKET: {
      requireVisible(world, command.point);
      RocketPad* rocket = dynamic_cast<RocketPad*>(
        world.map(command.point)->reportingConstruction);
      if(!rocket || rocket->stage != RocketPad::AWAITING)
        throw SimulationReplayError("rocket is not awaiting launch");
      rocket->launch_rocket();
      return SimulationCommandOutcome::APPLIED;
    }
    }
  } catch(const Message::Exception&) {
    throw SimulationReplayError("command was rejected by the world");
  }
  throw SimulationReplayError("unknown command type");
}

void recordSimulationDay(World& world, SimulationReplay& replay,
  std::uint32_t day, std::vector<SimulationCommand> commands
) {
  if(day == 0 || day != static_cast<std::uint32_t>(world.total_time + 1))
    throw SimulationReplayError("recorded day does not follow world time");
  for(SimulationCommand& command : commands) {
    command.day = day;
    command.phase = SimulationCommandPhase::BEFORE_STEP;
    command.sequence = replay.commands.empty()
      ? 1 : replay.commands.back().sequence + 1;
    command.expected = executeSimulationCommand(world, command);
    replay.commands.push_back(command);
  }
  world.do_time_step();
  replay.checkpoints.push_back({day,
    captureStateSnapshot(world).fingerprint()});
}

void replaySimulation(World& world, const SimulationReplay& replay) {
  validateReplay(replay);
  if(captureStateSnapshot(world).fingerprint() != replay.initialFingerprint)
    throw SimulationReplayError("initial world fingerprint does not match replay");

  std::size_t commandIndex = 0;
  for(const SimulationCheckpoint& checkpoint : replay.checkpoints) {
    if(checkpoint.day != static_cast<std::uint32_t>(world.total_time + 1))
      throw SimulationReplayError("checkpoint day does not follow world time");
    while(commandIndex < replay.commands.size()
      && replay.commands[commandIndex].day == checkpoint.day) {
      const SimulationCommand& command = replay.commands[commandIndex++];
      const SimulationCommandOutcome actual =
        executeSimulationCommand(world, command);
      if(actual != command.expected)
        throw SimulationReplayError("command outcome differs from recording");
    }
    if(commandIndex < replay.commands.size()
      && replay.commands[commandIndex].day < checkpoint.day)
      throw SimulationReplayError("command has no matching checkpoint");
    world.do_time_step();
    if(captureStateSnapshot(world).fingerprint() != checkpoint.fingerprint)
      throw SimulationReplayError("daily fingerprint differs from recording");
  }
  if(commandIndex != replay.commands.size())
    throw SimulationReplayError("command occurs after final checkpoint");
}

std::string serializeSimulationReplay(const SimulationReplay& replay) {
  validateReplay(replay);
  std::ostringstream out;
  out << "LINCITYNG-REPLAY\t" << replay.version << '\n'
      << "protocol\t" << replay.protocol << '\n'
      << "seed\t" << replay.seed << '\n'
      << "origin\t" << replay.origin << '\n'
      << "initial\t" << replay.initialFingerprint.hex() << '\n';
  for(const SimulationCommand& command : replay.commands) {
    out << "command\t" << command.day << "\tbefore-step\t"
        << command.sequence << '\t' << typeName(command.type) << '\t'
        << command.point.x << '\t' << command.point.y << '\t'
        << command.argument0 << '\t' << command.argument1 << '\t'
        << outcomeName(command.expected) << '\n';
  }
  for(const SimulationCheckpoint& checkpoint : replay.checkpoints)
    out << "checkpoint\t" << checkpoint.day << '\t'
        << checkpoint.fingerprint.hex() << '\n';
  return out.str();
}

SimulationReplay parseSimulationReplay(std::string_view bytes) {
  SimulationReplay replay;
  bool sawHeader = false, sawProtocol = false, sawSeed = false;
  bool sawOrigin = false, sawInitial = false;
  while(!bytes.empty()) {
    const std::size_t newline = bytes.find('\n');
    std::string_view line = bytes.substr(0, newline);
    if(newline == std::string_view::npos) bytes = {};
    else bytes.remove_prefix(newline + 1);
    if(line.empty()) continue;
    const std::vector<std::string_view> fields = split(line);
    if(fields[0] == "LINCITYNG-REPLAY" && fields.size() == 2) {
      if(sawHeader) throw SimulationReplayError("duplicate replay header");
      replay.version = parseInteger<std::uint32_t>(fields[1], "version");
      sawHeader = true;
    } else if(fields[0] == "protocol" && fields.size() == 2) {
      if(sawProtocol) throw SimulationReplayError("duplicate protocol");
      replay.protocol = std::string(fields[1]); sawProtocol = true;
    } else if(fields[0] == "seed" && fields.size() == 2) {
      if(sawSeed) throw SimulationReplayError("duplicate seed");
      replay.seed = parseInteger<std::uint64_t>(fields[1], "seed"); sawSeed = true;
    } else if(fields[0] == "origin" && fields.size() == 2) {
      if(sawOrigin) throw SimulationReplayError("duplicate origin");
      replay.origin = std::string(fields[1]); sawOrigin = true;
    } else if(fields[0] == "initial" && fields.size() == 2) {
      if(sawInitial) throw SimulationReplayError("duplicate initial fingerprint");
      replay.initialFingerprint = parseFingerprint(fields[1]); sawInitial = true;
    } else if(fields[0] == "command" && fields.size() == 10) {
      SimulationCommand command;
      command.day = parseInteger<std::uint32_t>(fields[1], "command day");
      if(fields[2] != "before-step")
        throw SimulationReplayError("unsupported command phase");
      command.sequence = parseInteger<std::uint32_t>(fields[3], "sequence");
      command.type = parseType(fields[4]);
      command.point.x = parseInteger<int>(fields[5], "x");
      command.point.y = parseInteger<int>(fields[6], "y");
      command.argument0 = parseInteger<int>(fields[7], "argument0");
      command.argument1 = parseInteger<int>(fields[8], "argument1");
      command.expected = parseOutcome(fields[9]);
      replay.commands.push_back(command);
    } else if(fields[0] == "checkpoint" && fields.size() == 3) {
      replay.checkpoints.push_back({
        parseInteger<std::uint32_t>(fields[1], "checkpoint day"),
        parseFingerprint(fields[2])});
    } else {
      throw SimulationReplayError("unknown or malformed replay record");
    }
  }
  if(!sawHeader || !sawProtocol || !sawSeed || !sawOrigin || !sawInitial)
    throw SimulationReplayError("replay header is incomplete");
  validateReplay(replay);
  return replay;
}
