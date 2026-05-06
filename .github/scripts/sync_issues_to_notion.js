// .github/scripts/sync-issue-to-notion.js
const { Client } = require("@notionhq/client")

const notion = new Client({ auth: process.env.NOTION_TOKEN })
const issue = JSON.parse(process.env.ISSUE_JSON)

const status =
  issue.state === "closed"
    ? "Done"
    : "Backlog"

async function main() {
  await notion.pages.create({
    parent: {
      database_id: process.env.NOTION_DATABASE_ID
    },
    properties: {
      Name: {
        title: [{ text: { content: issue.title } }]
      },
      Status: {
        select: { name: status }
      },
      "GitHub Issue": {
        url: issue.html_url
      },
      "Issue Number": {
        number: issue.number
      },
      Tags: {
        multi_select: issue.labels.map(label => ({
          name: label.name
        }))
      }
    },
    children: [
      {
        object: "block",
        type: "paragraph",
        paragraph: {
          rich_text: [
            {
              text: {
                content: issue.body || "No description."
              }
            }
          ]
        }
      }
    ]
  })
}

main().catch(err => {
  console.error("Notion sync failed:")
  console.error(JSON.stringify(err, null, 2))
  process.exit(1)
})
