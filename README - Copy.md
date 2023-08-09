# Backend code exercise


## Prerequisites
- Elixir 1.15.4
- Erlang OTP 26
- Postgres 15.3

## Project Setup Instructions
- Install/Update Hex using `mix local.hex`
- Install/Update Phoenix using `mix archive.install hex phx_new`
- Change the PostgreSQL credentials in `config/dev.exs`
- Setup and Seed the database using `mix ecto.setup`
- Run the server using `mix phx.server`
- Use the following cURL command to create a test user and get an auth token for the same
	- `curl --request POST \
  --url http://localhost:4000/api/v1/accounts/create \
  --header 'Content-Type: application/json' \
  --data '{
	"account": {
		"email": "test@test.com",
		"hashed_password": "test_password",
		"full_name": "Test Name"
	}
}'`
- Copy the provided token and include it in your API with the prefix `Bearer {{token}}` to authenticate yourself and enable access to the following APIs
- cURL for Task 1 API:
	- `curl --request GET \
  --url 'http://localhost:4000/api/v1/users?search_term=anna&search_field=full_name&limit=10&offset=0&order_by=full_name&order_dir=desc' \
  --header 'Authorization: Bearer {{token}}'`
- cURL for Task 2 API:
	- `curl --request POST \
  --url http://localhost:4000/api/v1/invite-users \
  --header 'Authorization: Bearer {{token}}'`

<p align="center">
<img src="https://github.com/chetanverma19/3DGameEngine/blob/main/tablediagramremote.png?raw=true" alt="Table Diagram" width="200"/>
</p>

## Implementation details

The following are some improvements that could have been made but the current scope was limited due to access to just a windows system at the moment, time and architecture limitations.
- Implement better full text indexing to allow for searching with fuzziness. (Implement MongoDB or ElasticSearch in parallel)
- Shift salary filtering to pure SQL rather than in code.
- Make the DB Schema more mature and scalable
- Implement and Test Bcrypt, limited due to only having access to a windows system. Facing issues with hashing libraries, including Argon2 and Pbkdf2, which have widespread issues with Windows Systems. **NOTE: The code for password hashing is included but commented**
- Shift sending emails in bulk (Task 2) to a PubSub, Queues and Worker System. The current system is limited in it's capabilities to ~100,000 emails. Will need more architecture freedom for >1,000,000.
- More robust authentication and authorization including but not limited to, is_staff and superadmin permissions, token expiry, access and refresh tokens.
- Implement a proper logger
- Weigh pros and cons and shift PG credentials for Dev to Environment Variables as well (Like Prod)

## Current Project Setup
- mix local.hex - To Update or Install hex
- mix archive.install hex phx_new - To Install Phoenix
- mix phx.new <project_name> --no-install –app <app_name> --database postgres --no-live --no-assets --no-html --no-dashboard --no-mailer --binary-id
   -  --no-install: To prevent automatic installation of dependencies, we want to make some changes to the DB setup and then install dependencies and setup database
   - --database: This will set up our database adapter for ecto for postgres
   - --no-live: This will comment out live socket view setup (Not Needed)
   - --no-assets: Will not generate the assets folder at all
   - --no-html: This will not generate HTML views
   - --no-dashboard: Not needed with current product requirements
   - --no-mailer: Will not generate swoosh mailer files (Custom mailer setup)
   - --binary-id: To switch ID from Integer to UUIDs
- mix phx.gen.json <module_name> <schema_file_name> <table_name> - To generate controller, view and context for required JSON resources

### Original File Starts Here

Hi there!

If you're reading this, it means you're now at the coding exercise step of the engineering hiring process. We're really happy that you made it here and super appreciative of your time!

In this exercise you're asked to create a Phoenix application and implement some features on it.

> 💡 The Phoenix application is an API

If you have any questions, don't hesitate to reach out directly to [code_exercise@remote.com](mailto:code_exercise@remote.com).

## Expectations

- It should be production-ready code - the code will show us how you ship things to production and be a mirror of your craft.
  - Just to be extra clear: We don't actually expect you to deploy it somewhere or build a release. It's meant as a statement regarding the quality of the solution.
- Take whatever time you need - we won’t look at start/end dates, you have a life besides this and we respect that! Moreover, if there is something you had to leave incomplete or there is a better solution you would implement but couldn’t due to personal time constraints, please try to walk us through your thought process or any missing parts, using the “Implementation Details” section below.

## What will you build

A phoenix app with 2 endpoints to manage users.

We don’t expect you to implement authentication and authorization but your final solution should assume it will be deployed to production and the data will be consumed by a Single Page Application that runs on customer’s browsers.

To save you some setup time we prepared this repo with a phoenix app that you can use to develop your solution. Alternatively, you can also generate a new phoenix project.

## Requirements

- We should store users and salaries in PostgreSQL database.
- Each user has a name and can have multiple salaries.
- Each salary should have a currency.
- Every field defined above should be required.
- One user should at most have 1 salary active at a given time.
- All endpoints should return JSON.
- A readme file with instructions on how to run the app.

### Seeding the database

- `mix ecto.setup` should create database tables, and seed the database with 20k users, for each user it should create 2 salaries with random amounts/currencies.
- The status of each salary should also be random, allowing for users without any active salary and for users with just 1 active salary.
- Must use 4 or more different currencies. Eg: USD, EUR, JPY and GBP.
- Users’ name can be random or populated from the result of calling list_names/0 defined in the following library: [https://github.com/remotecom/be_challengex](https://github.com/remotecom/be_challengex)

### Tasks

1. 📄 Implement an endpoint to provide a list of users and their salaries
    - Each user should return their `name` and active `salary`.
    - Some users might have been offboarded (offboarding functionality should be considered out of the scope for this exercise) so it’s possible that all salaries that belong to a user are inactive. In those cases, the endpoint is supposed to return the salary that was active most recently.
    - This endpoint should support filtering by partial user name and order by user name.
    - Endpoint: `GET /users`

2. 📬 Implement an endpoint that sends an email to all users with active salaries
    - The action of sending the email must use Remote’s Challenge lib: [https://github.com/remotecom/be_challengex](https://github.com/remotecom/be_challengex)
    - ⚠️ This library doesn’t actually send any email so you don’t necessarily need internet access to work on your challenge.
    - Endpoint: `POST /invite-users`

### When you're done

- You can use the "Implementation Details" section to explain some decisions/shortcomings of your implementation.
- Open a Pull Request in this repo and send the link to [code_exercise@remote.com](mailto:code_exercise@remote.com).
- You can also send some feedback about this exercise. Was it too big/short? Boring? Let us know!

---

## How to run the existing application

You will need the following installed:

- Elixir >= 1.14
- Postgres >= 14.5

Check out the `.tool-versions` file for a concrete version combination we ran the application with. Using [asdf](https://github.com/asdf-vm/asdf) you could install their plugins and them via `asdf install`.

### To start your Phoenix server

- Run `mix setup` to install, setup dependencies and setup the database
- Start Phoenix endpoint with `mix phx.server` or inside IEx with `iex -S mix phx.server`

Now you can visit [`localhost:4000`](http://localhost:4000) from your browser.

---

## Implementation details

This section is for you to fill in with any decisions you made that may be relevant. You can also change this README to fit your needs.